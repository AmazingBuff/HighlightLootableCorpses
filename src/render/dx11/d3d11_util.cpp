//
// Created by AmazingBuff on 2026/9/13.
//

#include "d3d11_util.h"

#include <REX/W32/D3DCOMPILER.h>

PLUGIN_NAMESPACE_BEGIN

REX::W32::ID3DBlob* compile_shader(char const* source, char const* entry, char const* target, char const* name, char const* log_prefix)
{
    if (!source || !entry || !target)
        return nullptr;

    REX::W32::ID3DBlob* blob = nullptr;
    REX::W32::ID3DBlob* err = nullptr;
    REX::W32::HRESULT const hr = REX::W32::D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target, 0, 0, &blob, &err);
    if (!REX::W32::SUCCESS(hr))
    {
        logger::error("{} shader compile failed [{} {}] ({:X}): {}",
            log_prefix ? log_prefix : "?",
            name ? name : "?",
            target,
            static_cast<unsigned int>(hr),
            err ? static_cast<char const*>(err->GetBufferPointer()) : "no diagnostics");
        if (blob)
        {
            blob->Release();
            blob = nullptr;
        }
    }
    if (err)
        err->Release();

    return blob;
}

D3D11StateCapture::D3D11StateCapture(REX::W32::ID3D11DeviceContext* context) :
    m_ref_context(context),
    m_render_targets{},
    m_unordered_access_views{},
    m_depth_stencil(nullptr),
    m_blend(nullptr),
    m_blend_factor{},
    m_sample_mask(0),
    m_depth(nullptr),
    m_stencil_ref(0),
    m_rasterizer(nullptr),
    m_viewport_count(0),
    m_viewports{},
    m_scissor_count(0),
    m_scissor_rects{},
    m_input_layout(nullptr),
    m_topology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
    m_vertex_buffers{},
    m_vertex_strides{},
    m_vertex_offsets{},
    m_index_buffer(nullptr),
    m_index_format(REX::W32::DXGI_FORMAT_UNKNOWN),
    m_index_offset(0),
    m_vertex_shader(nullptr),
    m_vertex_instances{},
    m_vertex_instance_count(Class_Instance_Count),
    m_pixel_shader(nullptr),
    m_pixel_instances{},
    m_pixel_instance_count(Class_Instance_Count),
    m_geometry_shader(nullptr),
    m_geometry_instances{},
    m_geometry_instance_count(Class_Instance_Count),
    m_hull_shader(nullptr),
    m_hull_instances{},
    m_hull_instance_count(Class_Instance_Count),
    m_domain_shader(nullptr),
    m_domain_instances{},
    m_domain_instance_count(Class_Instance_Count),
    m_vertex_cbs{},
    m_pixel_cbs{},
    m_pixel_srvs{},
    m_pixel_sampler(nullptr)
{
    capture();
}

D3D11StateCapture::~D3D11StateCapture()
{
    restore();

    for (REX::W32::ID3D11RenderTargetView* render_target : m_render_targets)
    {
        if (render_target)
            render_target->Release();
    }
    for (REX::W32::ID3D11UnorderedAccessView* unordered_access_view : m_unordered_access_views)
    {
        if (unordered_access_view)
            unordered_access_view->Release();
    }
    if (m_depth_stencil)
        m_depth_stencil->Release();
    if (m_blend)
        m_blend->Release();
    if (m_depth)
        m_depth->Release();
    if (m_rasterizer)
        m_rasterizer->Release();
    if (m_input_layout)
        m_input_layout->Release();
    for (REX::W32::ID3D11Buffer* vertex_buffer : m_vertex_buffers)
    {
        if (vertex_buffer)
            vertex_buffer->Release();
    }
    if (m_index_buffer)
        m_index_buffer->Release();
    if (m_vertex_shader)
        m_vertex_shader->Release();
    if (m_pixel_shader)
        m_pixel_shader->Release();
    for (REX::W32::ID3D11ClassInstance* instance : m_vertex_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_pixel_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_geometry_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_hull_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_domain_instances)
    {
        if (instance)
            instance->Release();
    }
    if (m_geometry_shader)
        m_geometry_shader->Release();
    if (m_hull_shader)
        m_hull_shader->Release();
    if (m_domain_shader)
        m_domain_shader->Release();
    for (REX::W32::ID3D11Buffer* cb : m_vertex_cbs)
    {
        if (cb)
            cb->Release();
    }
    for (REX::W32::ID3D11Buffer* cb : m_pixel_cbs)
    {
        if (cb)
            cb->Release();
    }
    for (REX::W32::ID3D11ShaderResourceView* srv : m_pixel_srvs)
    {
        if (srv)
            srv->Release();
    }
    if (m_pixel_sampler)
        m_pixel_sampler->Release();
}

void D3D11StateCapture::capture()
{
    if (!m_ref_context)
        return;

    m_ref_context->OMGetRenderTargetsAndUnorderedAccessViews(
        Render_Target_Count,
        m_render_targets,
        &m_depth_stencil,
        0,
        REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT,
        m_unordered_access_views);
    m_ref_context->OMGetBlendState(&m_blend, m_blend_factor, &m_sample_mask);
    m_ref_context->OMGetDepthStencilState(&m_depth, &m_stencil_ref);
    m_ref_context->RSGetState(&m_rasterizer);

    m_viewport_count = REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    m_ref_context->RSGetViewports(&m_viewport_count, m_viewports);
    m_scissor_count = REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    m_ref_context->RSGetScissorRects(&m_scissor_count, m_scissor_rects);

    m_ref_context->IAGetInputLayout(&m_input_layout);
    m_ref_context->IAGetPrimitiveTopology(&m_topology);
    m_ref_context->IAGetVertexBuffers(0, Vertex_Buffer_Count, m_vertex_buffers, m_vertex_strides, m_vertex_offsets);
    m_ref_context->IAGetIndexBuffer(&m_index_buffer, &m_index_format, &m_index_offset);

    m_ref_context->VSGetShader(&m_vertex_shader, m_vertex_instances, &m_vertex_instance_count);
    m_ref_context->PSGetShader(&m_pixel_shader, m_pixel_instances, &m_pixel_instance_count);
    m_ref_context->GSGetShader(&m_geometry_shader, m_geometry_instances, &m_geometry_instance_count);
    m_ref_context->HSGetShader(&m_hull_shader, m_hull_instances, &m_hull_instance_count);
    m_ref_context->DSGetShader(&m_domain_shader, m_domain_instances, &m_domain_instance_count);
    m_ref_context->VSGetConstantBuffers(0, 2, m_vertex_cbs);
    m_ref_context->PSGetConstantBuffers(0, 2, m_pixel_cbs);
    m_ref_context->PSGetShaderResources(0, 3, m_pixel_srvs);
    m_ref_context->PSGetSamplers(0, 1, &m_pixel_sampler);
}

void D3D11StateCapture::restore() const noexcept
{
    uint32_t render_target_count = 0;
    for (uint32_t index = 0; index < Render_Target_Count; ++index)
    {
        if (m_render_targets[index])
            render_target_count = index + 1;
    }

    REX::W32::ID3D11UnorderedAccessView* null_unordered_access_views[Max_Unordered_Access_View_Count]{};
    uint32_t keep_unordered_access_counts[Max_Unordered_Access_View_Count];
    for (uint32_t& count : keep_unordered_access_counts)
        count = std::numeric_limits<uint32_t>::max();
    m_ref_context->OMSetRenderTargetsAndUnorderedAccessViews(
        0,
        nullptr,
        m_depth_stencil,
        0,
        REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT,
        null_unordered_access_views,
        keep_unordered_access_counts);

    uint32_t const unordered_access_start = std::min(render_target_count, static_cast<uint32_t>(REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT));
    uint32_t const unordered_access_count = REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT - unordered_access_start;
    m_ref_context->OMSetRenderTargetsAndUnorderedAccessViews(
        render_target_count,
        m_render_targets,
        m_depth_stencil,
        unordered_access_start,
        unordered_access_count,
        unordered_access_count ? m_unordered_access_views + unordered_access_start : nullptr,
        keep_unordered_access_counts);
    m_ref_context->OMSetBlendState(m_blend, m_blend_factor, m_sample_mask);
    m_ref_context->OMSetDepthStencilState(m_depth, m_stencil_ref);
    m_ref_context->RSSetState(m_rasterizer);
    m_ref_context->RSSetViewports(m_viewport_count, m_viewports);
    m_ref_context->RSSetScissorRects(m_scissor_count, m_scissor_rects);
    m_ref_context->IASetInputLayout(m_input_layout);
    m_ref_context->IASetPrimitiveTopology(m_topology);
    m_ref_context->IASetVertexBuffers(0, Vertex_Buffer_Count, m_vertex_buffers, m_vertex_strides, m_vertex_offsets);
    m_ref_context->IASetIndexBuffer(m_index_buffer, m_index_format, m_index_offset);
    m_ref_context->VSSetShader(m_vertex_shader, m_vertex_instances, m_vertex_instance_count);
    m_ref_context->PSSetShader(m_pixel_shader, m_pixel_instances, m_pixel_instance_count);
    m_ref_context->GSSetShader(m_geometry_shader, m_geometry_instances, m_geometry_instance_count);
    m_ref_context->HSSetShader(m_hull_shader, m_hull_instances, m_hull_instance_count);
    m_ref_context->DSSetShader(m_domain_shader, m_domain_instances, m_domain_instance_count);
    m_ref_context->VSSetConstantBuffers(0, 2, m_vertex_cbs);
    m_ref_context->PSSetConstantBuffers(0, 2, m_pixel_cbs);
    m_ref_context->PSSetShaderResources(0, 3, m_pixel_srvs);
    m_ref_context->PSSetSamplers(0, 1, &m_pixel_sampler);
}

PLUGIN_NAMESPACE_END

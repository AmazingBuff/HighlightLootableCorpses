//
// Created by AmazingBuff on 2026/9/13.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN
[[nodiscard]] REX::W32::ID3DBlob* compile_shader(char const* source, char const* entry, char const* target, char const* name, char const* log_prefix);

class D3D11StateCapture
{
public:
    explicit D3D11StateCapture(REX::W32::ID3D11DeviceContext* context);
    ~D3D11StateCapture();

    D3D11StateCapture(D3D11StateCapture const&) = delete;
    D3D11StateCapture(D3D11StateCapture&&) = delete;
    D3D11StateCapture& operator=(D3D11StateCapture const&) = delete;
    D3D11StateCapture& operator=(D3D11StateCapture&&) = delete;
private:
    void capture();
    void restore() const noexcept;

    static constexpr uint32_t Render_Target_Count = REX::W32::D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    static constexpr uint32_t Max_Unordered_Access_View_Count = REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT;
    static constexpr uint32_t Vertex_Buffer_Count = 2;
    static constexpr uint32_t Class_Instance_Count = 256;

    REX::W32::ID3D11DeviceContext* m_ref_context;

    REX::W32::ID3D11RenderTargetView* m_render_targets[Render_Target_Count];
    REX::W32::ID3D11UnorderedAccessView* m_unordered_access_views[Max_Unordered_Access_View_Count];
    REX::W32::ID3D11DepthStencilView* m_depth_stencil;
    REX::W32::ID3D11BlendState* m_blend;
    float m_blend_factor[4];
    uint32_t m_sample_mask;
    REX::W32::ID3D11DepthStencilState* m_depth;
    uint32_t m_stencil_ref;
    REX::W32::ID3D11RasterizerState* m_rasterizer;
    uint32_t m_viewport_count;
    REX::W32::D3D11_VIEWPORT m_viewports[REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    uint32_t m_scissor_count;
    REX::W32::D3D11_RECT m_scissor_rects[REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    REX::W32::ID3D11InputLayout* m_input_layout;
    REX::W32::D3D11_PRIMITIVE_TOPOLOGY m_topology;
    REX::W32::ID3D11Buffer* m_vertex_buffers[Vertex_Buffer_Count];
    uint32_t m_vertex_strides[Vertex_Buffer_Count];
    uint32_t m_vertex_offsets[Vertex_Buffer_Count];
    REX::W32::ID3D11Buffer* m_index_buffer;
    REX::W32::DXGI_FORMAT m_index_format;
    uint32_t m_index_offset;
    REX::W32::ID3D11VertexShader* m_vertex_shader;
    REX::W32::ID3D11ClassInstance* m_vertex_instances[Class_Instance_Count];
    uint32_t m_vertex_instance_count;
    REX::W32::ID3D11PixelShader* m_pixel_shader;
    REX::W32::ID3D11ClassInstance* m_pixel_instances[Class_Instance_Count];
    uint32_t m_pixel_instance_count;
    REX::W32::ID3D11GeometryShader* m_geometry_shader;
    REX::W32::ID3D11ClassInstance* m_geometry_instances[Class_Instance_Count];
    uint32_t m_geometry_instance_count;
    REX::W32::ID3D11HullShader* m_hull_shader;
    REX::W32::ID3D11ClassInstance* m_hull_instances[Class_Instance_Count];
    uint32_t m_hull_instance_count;
    REX::W32::ID3D11DomainShader* m_domain_shader;
    REX::W32::ID3D11ClassInstance* m_domain_instances[Class_Instance_Count];
    uint32_t m_domain_instance_count;
    REX::W32::ID3D11Buffer* m_vertex_cbs[2];
    REX::W32::ID3D11Buffer* m_pixel_cbs[2];
    REX::W32::ID3D11ShaderResourceView* m_pixel_srvs[3];
    REX::W32::ID3D11SamplerState* m_pixel_sampler;
};

PLUGIN_NAMESPACE_END

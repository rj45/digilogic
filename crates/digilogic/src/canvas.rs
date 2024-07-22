use vello::*;
use wgpu::*;

#[inline]
const fn egui_to_vello_color(color: egui::Color32) -> vello::peniko::Color {
    vello::peniko::Color {
        r: color.r(),
        g: color.g(),
        b: color.b(),
        a: color.a(),
    }
}

pub struct Canvas {
    texture: Texture,
    texture_view: TextureView,
    texture_id: egui::TextureId,
    renderer: Renderer,
}

const TEXTURE_FILTER: FilterMode = FilterMode::Nearest;
const ANTIALIASING_METHOD: AaConfig = AaConfig::Area;

fn create_texture(
    render_state: &egui_wgpu::RenderState,
    width: u32,
    height: u32,
) -> (Texture, TextureView) {
    let desc = TextureDescriptor {
        label: Some("Canvas"),
        size: Extent3d {
            width,
            height,
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: TextureDimension::D2,
        format: TextureFormat::Rgba8Unorm,
        usage: TextureUsages::TEXTURE_BINDING | TextureUsages::STORAGE_BINDING,
        view_formats: &[],
    };

    let texture = render_state.device.create_texture(&desc);
    let texture_view = texture.create_view(&TextureViewDescriptor::default());
    (texture, texture_view)
}

impl Canvas {
    pub fn create(render_state: &egui_wgpu::RenderState, width: u32, height: u32) -> Self {
        let (texture, texture_view) = create_texture(render_state, width, height);

        let texture_id = render_state.renderer.write().register_native_texture(
            &render_state.device,
            &texture_view,
            TEXTURE_FILTER,
        );

        let renderer = Renderer::new(
            &render_state.device,
            RendererOptions {
                surface_format: None,
                use_cpu: false,
                antialiasing_support: std::iter::once(ANTIALIASING_METHOD).collect(),

                #[cfg(not(target_os = "macos"))]
                num_init_threads: None,
                #[cfg(target_os = "macos")]
                num_init_threads: std::num::NonZeroUsize::new(1),
            },
        )
        .unwrap();

        Self {
            texture,
            texture_view,
            texture_id,
            renderer,
        }
    }

    #[inline]
    pub fn width(&self) -> u32 {
        self.texture.width()
    }

    #[inline]
    pub fn height(&self) -> u32 {
        self.texture.height()
    }

    #[inline]
    pub fn texture_id(&self) -> egui::TextureId {
        self.texture_id
    }

    pub fn resize(&mut self, render_state: &egui_wgpu::RenderState, width: u32, height: u32) {
        if (self.width() == width) && (self.height() == height) {
            return;
        }

        (self.texture, self.texture_view) = create_texture(render_state, width, height);

        render_state
            .renderer
            .write()
            .update_egui_texture_from_wgpu_texture(
                &render_state.device,
                &self.texture_view,
                TEXTURE_FILTER,
                self.texture_id,
            );
    }

    pub fn render(
        &mut self,
        render_state: &egui_wgpu::RenderState,
        scene: &Scene,
        background: egui::Color32,
    ) {
        self.renderer
            .render_to_texture(
                &render_state.device,
                &render_state.queue,
                scene,
                &self.texture_view,
                &RenderParams {
                    base_color: egui_to_vello_color(background),
                    width: self.width(),
                    height: self.height(),
                    antialiasing_method: ANTIALIASING_METHOD,
                },
            )
            .unwrap();
    }
}
use bevy::{prelude::*, window::PresentMode};

fn setup(mut commands: Commands, asset_server: Res<AssetServer>) {
    let font = asset_server.load("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf");
    let text_style = TextStyle {
        font,
        font_size: 60.0,
        color: Color::WHITE,
    };
    let text_alignment = TextAlignment::CENTER;

    commands.spawn_bundle(Camera2dBundle::default());

    commands.spawn_bundle(Text2dBundle {
        text: Text::from_section("Hellorld!", text_style.clone()).with_alignment(text_alignment),
        ..default()
    });
}

fn main() {
    App::new()
        .insert_resource(WindowDescriptor {
            title: "digilogic".to_string(),
            width: 500.,
            height: 300.,
            present_mode: PresentMode::AutoVsync,
            ..default()
        })
        .insert_resource(Msaa { samples: 4 })
        .insert_resource(ClearColor(Color::rgb(0.1, 0.1, 0.15)))
        .add_plugins(DefaultPlugins)
        .add_startup_system(setup)
        .run();
}

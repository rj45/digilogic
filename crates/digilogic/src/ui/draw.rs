use bevy_ecs::prelude::*;
use digilogic_core::components::Shape;
use vello::kurbo::Affine;

#[derive(Default, Resource)]
pub struct Scene(pub vello::Scene);

#[derive(Default, Resource)]
pub struct SymbolSVGs(pub Vec<vello::Scene>);

pub fn draw(mut scene: ResMut<Scene>, symbol_svgs: Res<SymbolSVGs>) {
    let scene = &mut scene.0;
    scene.reset();

    scene.append(
        &symbol_svgs.0[Shape::And as usize],
        Some(Affine::scale(2.0).then_translate((100.0, 100.0).into())),
    );

    scene.append(
        &symbol_svgs.0[Shape::Or as usize],
        Some(Affine::scale(2.0).then_translate((100.0, 200.0).into())),
    );

    scene.append(
        &symbol_svgs.0[Shape::Xor as usize],
        Some(Affine::scale(2.0).then_translate((100.0, 300.0).into())),
    );

    scene.append(
        &symbol_svgs.0[Shape::Not as usize],
        Some(Affine::scale(2.0).then_translate((100.0, 400.0).into())),
    );

    scene.append(
        &symbol_svgs.0[Shape::Input as usize],
        Some(Affine::scale(2.0).then_translate((100.0, 500.0).into())),
    );

    scene.append(
        &symbol_svgs.0[Shape::Output as usize],
        Some(Affine::scale(2.0).then_translate((100.0, 600.0).into())),
    );
}

pub fn init_symbol_shapes(mut symbol_svgs: ResMut<SymbolSVGs>) {
    use vello_svg::usvg::{Options, Size, Tree};
    // Chip
    let svg = Tree::from_str("<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1\" height=\"1\"/>", &Options::default()).unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // Port
    let svg = Tree::from_str("<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg xmlns=\"http://www.w3.org/2000/svg\" width=\"3\" height=\"3\"><circle cx=\"1.5\" cy=\"1.5\" r=\"1.5\" /></svg>", &Options::default()).unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // And
    let svg = Tree::from_data(
        include_bytes!("../../assets/schemalib/symbols/schemalib-and2-l.svg"),
        &Default::default(),
    )
    .unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // Or
    let svg = Tree::from_data(
        include_bytes!("../../assets/schemalib/symbols/schemalib-or2-l.svg"),
        &Default::default(),
    )
    .unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // Xor
    let svg = Tree::from_data(
        include_bytes!("../../assets/schemalib/symbols/schemalib-xor2-l.svg"),
        &Default::default(),
    )
    .unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // Not
    let svg = Tree::from_data(
        include_bytes!("../../assets/schemalib/symbols/schemalib-inv-l.svg"),
        &Default::default(),
    )
    .unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // Input
    let svg = Tree::from_data(
        include_bytes!("../../assets/schemalib/symbols/digilogic-input-l.svg"),
        &Default::default(),
    )
    .unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));

    // Output
    let svg = Tree::from_data(
        include_bytes!("../../assets/schemalib/symbols/digilogic-output-l.svg"),
        &Default::default(),
    )
    .unwrap();
    symbol_svgs.0.push(vello_svg::render_tree(&svg));
}

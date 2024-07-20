use crate::c::{
    Box, DrawContext, DrawFlags, DrawLabelType, FontHandle, HMM_Vec2, HorizAlign, SymbolShape,
    Theme, VertAlign,
};

#[derive(Copy, Clone)]
pub struct Draw {
    // TODO: set up a vec of shapes that can be drawn by egui
    // pub shapes: Vec<egui::Shape>,
}

#[derive(Copy, Clone)]
pub struct Font {}

// TODO: implement the Default trait for Draw
// impl Default for Draw {
//     fn default() -> Self {}
// }

pub fn draw_ctx(draw: &mut Draw) -> *mut DrawContext {
    draw as *mut _ as *mut DrawContext
}

pub fn draw_font(font: &mut Font) -> FontHandle {
    font as *mut _ as FontHandle
}

// TODO: implement a matrix for the draw context that all coords can be transformed with
pub extern "C" fn draw_reset(draw: *mut DrawContext) {}

pub extern "C" fn draw_set_zoom(draw: *mut DrawContext, zoom: f32) {}

pub extern "C" fn draw_add_pan(draw: *mut DrawContext, pan: HMM_Vec2) {}

pub extern "C" fn draw_get_pan(draw: *mut DrawContext) -> HMM_Vec2 {
    HMM_Vec2 {
        Elements: [0.0, 0.0],
    }
}

pub extern "C" fn draw_get_zoom(draw: *mut DrawContext) -> f32 {
    0.0
}

pub extern "C" fn draw_screen_to_world(draw: *mut DrawContext, screenPos: HMM_Vec2) -> HMM_Vec2 {
    HMM_Vec2 {
        Elements: [0.0, 0.0],
    }
}

pub extern "C" fn draw_scale_screen_to_world(draw: *mut DrawContext, dirvec: HMM_Vec2) -> HMM_Vec2 {
    HMM_Vec2 {
        Elements: [0.0, 0.0],
    }
}

pub extern "C" fn draw_world_to_screen(draw: *mut DrawContext, worldPos: HMM_Vec2) -> HMM_Vec2 {
    HMM_Vec2 {
        Elements: [0.0, 0.0],
    }
}

pub extern "C" fn draw_scale_world_to_screen(draw: *mut DrawContext, dirvec: HMM_Vec2) -> HMM_Vec2 {
    HMM_Vec2 {
        Elements: [0.0, 0.0],
    }
}

pub extern "C" fn draw_symbol_shape(
    draw: *mut DrawContext,
    theme: *mut Theme,
    box_: Box,
    shape: SymbolShape,
    flags: DrawFlags,
) {
    // TODO: implement the symbol shape drawing by transforming the Box to screen space with a matrix transform, then
    // adding the shape to the draw context for egui to later draw
}

pub extern "C" fn draw_port(
    draw: *mut DrawContext,
    theme: *mut Theme,
    center: HMM_Vec2,
    flags: DrawFlags,
) {
}

pub extern "C" fn draw_selection_box(
    draw: *mut DrawContext,
    theme: *mut Theme,
    box_: Box,
    flags: DrawFlags,
) {
}

pub extern "C" fn draw_wire(
    draw: *mut DrawContext,
    theme: *mut Theme,
    verts: *mut HMM_Vec2,
    numVerts: ::std::os::raw::c_int,
    flags: DrawFlags,
) {
}

pub extern "C" fn draw_junction(
    draw: *mut DrawContext,
    theme: *mut Theme,
    pos: HMM_Vec2,
    flags: DrawFlags,
) {
}

pub extern "C" fn draw_waypoint(
    draw: *mut DrawContext,
    theme: *mut Theme,
    pos: HMM_Vec2,
    flags: DrawFlags,
) {
}

pub extern "C" fn draw_label(
    draw: *mut DrawContext,
    theme: *mut Theme,
    box_: Box,
    text: *const ::std::os::raw::c_char,
    type_: DrawLabelType,
    flags: DrawFlags,
) {
}

pub extern "C" fn draw_text_bounds(
    draw: *mut DrawContext,
    pos: HMM_Vec2,
    text: *const ::std::os::raw::c_char,
    len: ::std::os::raw::c_int,
    horz: HorizAlign,
    vert: VertAlign,
    fontSize: f32,
    font: FontHandle,
) -> Box {
    Box {
        center: HMM_Vec2 {
            Elements: [0.0, 0.0],
        },
        halfSize: HMM_Vec2 {
            Elements: [0.0, 0.0],
        },
    }
}

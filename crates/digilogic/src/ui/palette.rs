use bevy_ecs::prelude::*;
use bevy_reflect::prelude::*;
use bitflags::bitflags;
use serde::{Deserialize, Serialize};
use smallvec::smallvec;
use std::num::NonZeroU8;
use vello::kurbo::{Affine, Point};
use vello::peniko::*;

#[derive(Debug, Serialize, Deserialize, Reflect, Resource)]
pub struct Palette {
    pub logic_0_color: [u8; 3],
    pub logic_1_color: [u8; 3],
    pub high_z_color: [u8; 3],
    pub undefined_color: [u8; 3],
}

impl Default for Palette {
    fn default() -> Self {
        Self {
            logic_0_color: [20, 110, 35],
            logic_1_color: [40, 220, 70],
            high_z_color: [80, 80, 90],
            undefined_color: [200, 220, 50],
        }
    }
}

#[derive(Resource)]
pub struct PaletteBrushes {
    logic_0_color: Color,
    logic_1_color: Color,
    high_z_color: Color,
    undefined_color: Color,

    logic_0_logic_1_gradient: Gradient,
    logic_0_high_z_gradient: Gradient,
    logic_0_undefined_gradient: Gradient,
    logic_1_high_z_gradient: Gradient,
    logic_1_undefined_gradient: Gradient,
    high_z_undefined_gradient: Gradient,

    logic_0_logic_1_high_z_gradient: Gradient,
    logic_0_logic_1_undefined_gradient: Gradient,
    logic_0_high_z_undefined_gradient: Gradient,
    logic_1_high_z_undefined_gradient: Gradient,

    all_gradient: Gradient,

    brush_translation: f64,
}

impl Default for PaletteBrushes {
    fn default() -> Self {
        Self {
            logic_0_color: Color::MAGENTA,
            logic_1_color: Color::MAGENTA,
            high_z_color: Color::MAGENTA,
            undefined_color: Color::MAGENTA,

            logic_0_logic_1_gradient: Gradient::default(),
            logic_0_high_z_gradient: Gradient::default(),
            logic_0_undefined_gradient: Gradient::default(),
            logic_1_high_z_gradient: Gradient::default(),
            logic_1_undefined_gradient: Gradient::default(),
            high_z_undefined_gradient: Gradient::default(),

            logic_0_logic_1_high_z_gradient: Gradient::default(),
            logic_0_logic_1_undefined_gradient: Gradient::default(),
            logic_0_high_z_undefined_gradient: Gradient::default(),
            logic_1_high_z_undefined_gradient: Gradient::default(),

            all_gradient: Gradient::default(),

            brush_translation: 0.0,
        }
    }
}

const GRADIENT_COLOR_SIZE: f64 = 5.0;

fn two_color_gradient(colors: [Color; 2]) -> Gradient {
    Gradient {
        kind: GradientKind::Linear {
            start: Point::ZERO,
            end: Point::new(GRADIENT_COLOR_SIZE * 2.0, GRADIENT_COLOR_SIZE * 2.0),
        },
        extend: Extend::Repeat,
        stops: smallvec![
            ColorStop {
                offset: 0.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0 / 4.0 - 1.0 / 8.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0 / 4.0 + 1.0 / 8.0,
                color: colors[1],
            },
            ColorStop {
                offset: 3.0 / 4.0 - 1.0 / 8.0,
                color: colors[1],
            },
            ColorStop {
                offset: 3.0 / 4.0 + 1.0 / 8.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0,
                color: colors[0],
            },
        ],
    }
}

fn three_color_gradient(colors: [Color; 3]) -> Gradient {
    Gradient {
        kind: GradientKind::Linear {
            start: Point::ZERO,
            end: Point::new(GRADIENT_COLOR_SIZE * 3.0, GRADIENT_COLOR_SIZE * 3.0),
        },
        extend: Extend::Repeat,
        stops: smallvec![
            ColorStop {
                offset: 0.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0 / 6.0 - 1.0 / 12.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0 / 6.0 + 1.0 / 12.0,
                color: colors[1],
            },
            ColorStop {
                offset: 3.0 / 6.0 - 1.0 / 12.0,
                color: colors[1],
            },
            ColorStop {
                offset: 3.0 / 6.0 + 1.0 / 12.0,
                color: colors[2],
            },
            ColorStop {
                offset: 5.0 / 6.0 - 1.0 / 12.0,
                color: colors[2],
            },
            ColorStop {
                offset: 5.0 / 6.0 + 1.0 / 12.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0,
                color: colors[0],
            },
        ],
    }
}

fn four_color_gradient(colors: [Color; 4]) -> Gradient {
    Gradient {
        kind: GradientKind::Linear {
            start: Point::ZERO,
            end: Point::new(GRADIENT_COLOR_SIZE * 4.0, GRADIENT_COLOR_SIZE * 4.0),
        },
        extend: Extend::Repeat,
        stops: smallvec![
            ColorStop {
                offset: 0.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0 / 8.0 - 1.0 / 16.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0 / 8.0 + 1.0 / 16.0,
                color: colors[1],
            },
            ColorStop {
                offset: 3.0 / 8.0 - 1.0 / 16.0,
                color: colors[1],
            },
            ColorStop {
                offset: 3.0 / 8.0 + 1.0 / 16.0,
                color: colors[2],
            },
            ColorStop {
                offset: 5.0 / 8.0 - 1.0 / 16.0,
                color: colors[2],
            },
            ColorStop {
                offset: 5.0 / 8.0 + 1.0 / 16.0,
                color: colors[3],
            },
            ColorStop {
                offset: 7.0 / 8.0 - 1.0 / 16.0,
                color: colors[3],
            },
            ColorStop {
                offset: 7.0 / 8.0 + 1.0 / 16.0,
                color: colors[0],
            },
            ColorStop {
                offset: 1.0,
                color: colors[0],
            },
        ],
    }
}

fn update_palette_colors(palette: Res<Palette>, mut brushes: ResMut<PaletteBrushes>) {
    brushes.logic_0_color = Color::rgb8(
        palette.logic_0_color[0],
        palette.logic_0_color[1],
        palette.logic_0_color[2],
    );

    brushes.logic_1_color = Color::rgb8(
        palette.logic_1_color[0],
        palette.logic_1_color[1],
        palette.logic_1_color[2],
    );

    brushes.high_z_color = Color::rgb8(
        palette.high_z_color[0],
        palette.high_z_color[1],
        palette.high_z_color[2],
    );

    brushes.undefined_color = Color::rgb8(
        palette.undefined_color[0],
        palette.undefined_color[1],
        palette.undefined_color[2],
    );

    brushes.logic_0_logic_1_gradient =
        two_color_gradient([brushes.logic_0_color, brushes.logic_1_color]);
    brushes.logic_0_high_z_gradient =
        two_color_gradient([brushes.logic_0_color, brushes.high_z_color]);
    brushes.logic_0_undefined_gradient =
        two_color_gradient([brushes.logic_0_color, brushes.undefined_color]);
    brushes.logic_1_high_z_gradient =
        two_color_gradient([brushes.logic_1_color, brushes.high_z_color]);
    brushes.logic_1_undefined_gradient =
        two_color_gradient([brushes.logic_1_color, brushes.undefined_color]);
    brushes.high_z_undefined_gradient =
        two_color_gradient([brushes.high_z_color, brushes.undefined_color]);

    brushes.logic_0_logic_1_high_z_gradient = three_color_gradient([
        brushes.logic_0_color,
        brushes.logic_1_color,
        brushes.high_z_color,
    ]);
    brushes.logic_0_logic_1_undefined_gradient = three_color_gradient([
        brushes.logic_0_color,
        brushes.logic_1_color,
        brushes.undefined_color,
    ]);
    brushes.logic_0_high_z_undefined_gradient = three_color_gradient([
        brushes.logic_0_color,
        brushes.high_z_color,
        brushes.undefined_color,
    ]);
    brushes.logic_1_high_z_undefined_gradient = three_color_gradient([
        brushes.logic_1_color,
        brushes.high_z_color,
        brushes.undefined_color,
    ]);

    brushes.all_gradient = four_color_gradient([
        brushes.logic_0_color,
        brushes.logic_1_color,
        brushes.high_z_color,
        brushes.undefined_color,
    ]);
}

fn update_brush_translation(time: Res<bevy_time::Time>, mut brushes: ResMut<PaletteBrushes>) {
    const TRANSLATION_PER_SECOND: f64 = 5.0;
    const TRANSLATION_MODULUS: f64 = GRADIENT_COLOR_SIZE * 2.0 * 3.0 * 4.0;

    brushes.brush_translation += TRANSLATION_PER_SECOND * time.delta_seconds_f64();
    brushes.brush_translation %= TRANSLATION_MODULUS;
}

bitflags! {
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    struct LogicBitStates: u8 {
        const LOGIC_0 = 0x1;
        const LOGIC_1 = 0x2;
        const HIGH_Z = 0x4;
        const UNDEFINED = 0x8;

        const LOGIC_0_LOGIC_1   = Self::LOGIC_0.bits() | Self::LOGIC_1.bits();
        const LOGIC_0_HIGH_Z    = Self::LOGIC_0.bits() | Self::HIGH_Z.bits();
        const LOGIC_0_UNDEFINED = Self::LOGIC_0.bits() | Self::UNDEFINED.bits();
        const LOGIC_1_HIGH_Z    = Self::LOGIC_1.bits() | Self::HIGH_Z.bits();
        const LOGIC_1_UNDEFINED = Self::LOGIC_1.bits() | Self::UNDEFINED.bits();
        const HIGH_Z_UNDEFINED  = Self::HIGH_Z.bits() | Self::UNDEFINED.bits();

        const LOGIC_0_LOGIC_1_HIGH_Z    = Self::LOGIC_0.bits() | Self::LOGIC_1.bits() | Self::HIGH_Z.bits();
        const LOGIC_0_LOGIC_1_UNDEFINED = Self::LOGIC_0.bits() | Self::LOGIC_1.bits() | Self::UNDEFINED.bits();
        const LOGIC_0_HIGH_Z_UNDEFINED  = Self::LOGIC_0.bits() | Self::HIGH_Z.bits() | Self::UNDEFINED.bits();
        const LOGIC_1_HIGH_Z_UNDEFINED  = Self::LOGIC_1.bits() | Self::HIGH_Z.bits() | Self::UNDEFINED.bits();

        const ALL = Self::LOGIC_0.bits() | Self::LOGIC_1.bits() | Self::HIGH_Z.bits() | Self::UNDEFINED.bits();
    }
}

fn get_occurring_bit_states(
    bit_plane_0: &[u8],
    bit_plane_1: &[u8],
    width: NonZeroU8,
) -> LogicBitStates {
    let byte_count = width.get().div_ceil(8) as usize;
    let mut last_byte_bit_count = width.get() % 8;
    if last_byte_bit_count == 0 {
        last_byte_bit_count = 8;
    }

    let mut states = LogicBitStates::empty();

    for (&byte0, &byte1) in bit_plane_0.iter().zip(bit_plane_1).take(byte_count - 1) {
        if (!byte0 & byte1) > 0 {
            states |= LogicBitStates::LOGIC_0;
        }

        if (byte0 & byte1) > 0 {
            states |= LogicBitStates::LOGIC_1;
        }

        if (!byte0 & !byte1) > 0 {
            states |= LogicBitStates::HIGH_Z;
        }

        if (byte0 & !byte1) > 0 {
            states |= LogicBitStates::UNDEFINED;
        }
    }

    {
        let mask = ((1u16 << last_byte_bit_count) - 1) as u8;
        let byte0 = bit_plane_0[byte_count - 1];
        let byte1 = bit_plane_1[byte_count - 1];

        if ((!byte0 & byte1) & mask) > 0 {
            states |= LogicBitStates::LOGIC_0;
        }

        if ((byte0 & byte1) & mask) > 0 {
            states |= LogicBitStates::LOGIC_1;
        }

        if ((!byte0 & !byte1) & mask) > 0 {
            states |= LogicBitStates::HIGH_Z;
        }

        if ((byte0 & !byte1) & mask) > 0 {
            states |= LogicBitStates::UNDEFINED;
        }
    }

    states
}

impl PaletteBrushes {
    pub fn get_brush_for_state(
        &self,
        sim_state: Option<&digilogic_netcode::SimState>,
        offset: Option<digilogic_netcode::StateOffset>,
        width: Option<digilogic_core::components::BitWidth>,
    ) -> Option<BrushRef> {
        const MAX_BIT_PLANE_SIZE: usize = 32;
        let mut bit_plane_0 = [0u8; MAX_BIT_PLANE_SIZE];
        let mut bit_plane_1 = [0u8; MAX_BIT_PLANE_SIZE];

        sim_state.zip(offset).map(|(sim_state, offset)| {
            let width = width.map(|width| width.0).unwrap_or(NonZeroU8::MIN);
            sim_state.get_net(offset.0, width, &mut bit_plane_0, &mut bit_plane_1);

            let bit_states = get_occurring_bit_states(&bit_plane_0, &bit_plane_1, width);
            match bit_states {
                LogicBitStates::LOGIC_0 => self.logic_0_color.into(),
                LogicBitStates::LOGIC_1 => self.logic_1_color.into(),
                LogicBitStates::HIGH_Z => self.high_z_color.into(),
                LogicBitStates::UNDEFINED => self.undefined_color.into(),
                LogicBitStates::LOGIC_0_LOGIC_1 => (&self.logic_0_logic_1_gradient).into(),
                LogicBitStates::LOGIC_0_HIGH_Z => (&self.logic_0_high_z_gradient).into(),
                LogicBitStates::LOGIC_0_UNDEFINED => (&self.logic_0_undefined_gradient).into(),
                LogicBitStates::LOGIC_1_HIGH_Z => (&self.logic_1_high_z_gradient).into(),
                LogicBitStates::LOGIC_1_UNDEFINED => (&self.logic_1_undefined_gradient).into(),
                LogicBitStates::HIGH_Z_UNDEFINED => (&self.high_z_undefined_gradient).into(),
                LogicBitStates::LOGIC_0_LOGIC_1_HIGH_Z => {
                    (&self.logic_0_logic_1_high_z_gradient).into()
                }
                LogicBitStates::LOGIC_0_LOGIC_1_UNDEFINED => {
                    (&self.logic_0_logic_1_undefined_gradient).into()
                }
                LogicBitStates::LOGIC_0_HIGH_Z_UNDEFINED => {
                    (&self.logic_0_high_z_undefined_gradient).into()
                }
                LogicBitStates::LOGIC_1_HIGH_Z_UNDEFINED => {
                    (&self.logic_1_high_z_undefined_gradient).into()
                }
                LogicBitStates::ALL => (&self.all_gradient).into(),
                _ => unreachable!(),
            }
        })
    }

    pub fn get_color_for_state(
        &self,
        sim_state: Option<&digilogic_netcode::SimState>,
        offset: Option<digilogic_netcode::StateOffset>,
        width: Option<digilogic_core::components::BitWidth>,
    ) -> Option<Color> {
        const MAX_BIT_PLANE_SIZE: usize = 32;
        let mut bit_plane_0 = [0u8; MAX_BIT_PLANE_SIZE];
        let mut bit_plane_1 = [0u8; MAX_BIT_PLANE_SIZE];

        sim_state.zip(offset).and_then(|(sim_state, offset)| {
            let width = width.map(|width| width.0).unwrap_or(NonZeroU8::MIN);
            sim_state.get_net(offset.0, width, &mut bit_plane_0, &mut bit_plane_1);

            let bit_states = get_occurring_bit_states(&bit_plane_0, &bit_plane_1, width);
            match bit_states {
                LogicBitStates::LOGIC_0 => Some(self.logic_0_color),
                LogicBitStates::LOGIC_1 => Some(self.logic_1_color),
                LogicBitStates::HIGH_Z => Some(self.high_z_color),
                LogicBitStates::UNDEFINED => Some(self.undefined_color),
                _ => None,
            }
        })
    }

    pub fn get_brush_transform(&self) -> Affine {
        Affine::translate((self.brush_translation, self.brush_translation))
    }
}

#[derive(Debug, Default)]
pub struct PalettePlugin;

impl bevy_app::Plugin for PalettePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<Palette>()
            .init_resource::<Palette>()
            .init_resource::<PaletteBrushes>();

        app.add_systems(
            bevy_app::PreUpdate,
            update_palette_colors.run_if(resource_changed::<Palette>),
        );

        app.add_systems(bevy_app::Update, update_brush_translation);
    }
}

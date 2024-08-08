use aery::prelude::*;
use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_ecs::system::lifetimeless::{Read, Write};
use bevy_reflect::Reflect;

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, Hash, Component, Reflect)]
pub enum Visibility {
    #[default]
    Inherit,
    Visible,
    Hidden,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deref, Component, Reflect)]
#[repr(transparent)]
pub struct ComputedVisibility(bool);

impl Default for ComputedVisibility {
    #[inline]
    fn default() -> Self {
        Self(true)
    }
}

#[derive(Default, Debug, Bundle)]
pub struct VisibilityBundle {
    pub visibility: Visibility,
    pub computed_visibility: ComputedVisibility,
}

#[derive(Debug, Relation)]
pub struct InheritVisibility;

type RootQuery<'w, 's> = Query<
    'w,
    's,
    (Read<Visibility>, Write<ComputedVisibility>),
    Or<(Root<InheritVisibility>, Abstains<InheritVisibility>)>,
>;

fn update_root_visibility(mut roots: RootQuery) {
    for (visibility, mut computed_visibility) in roots.iter_mut() {
        let new_visibility = match visibility {
            Visibility::Inherit | Visibility::Visible => true,
            Visibility::Hidden => false,
        };

        if computed_visibility.0 != new_visibility {
            computed_visibility.0 = new_visibility;
        }
    }
}

fn update_visibility(
    mut tree: Query<(
        (&Visibility, &mut ComputedVisibility),
        Relations<InheritVisibility>,
    )>,
    roots: Query<Entity, Root<InheritVisibility>>,
) {
    tree.traverse_mut::<InheritVisibility>(roots.iter())
        .track_self()
        .for_each(
            |(_, parent_computed_visibility),
             _,
             (child_visibility, child_computed_visibility),
             _| {
                let new_visibility = match child_visibility {
                    Visibility::Inherit => parent_computed_visibility.0,
                    Visibility::Visible => true,
                    Visibility::Hidden => false,
                };

                if child_computed_visibility.0 != new_visibility {
                    child_computed_visibility.0 = new_visibility;
                }
            },
        );
}

pub(crate) struct VisibilityPlugin;

impl bevy_app::Plugin for VisibilityPlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.register_type::<Visibility>()
            .register_type::<ComputedVisibility>();

        app.register_relation::<InheritVisibility>();
        app.add_systems(
            bevy_app::PostUpdate,
            (update_root_visibility, update_visibility).chain(),
        );
    }
}

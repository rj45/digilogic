use bevy_derive::Deref;
use bevy_ecs::prelude::*;
use bevy_hierarchy::Parent;
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

#[derive(Default, Bundle)]
pub struct VisibilityBundle {
    pub visibility: Visibility,
    pub computed_visibility: ComputedVisibility,
}

pub(crate) fn update_visibility(
    mut computed: Query<(&Visibility, &mut ComputedVisibility, Option<&Parent>)>,
    ancestors: Query<(Option<&Visibility>, Option<&Parent>), Or<(With<Visibility>, With<Parent>)>>,
) {
    computed
        .par_iter_mut()
        .for_each(|(vis, mut comp_vis, parent)| {
            let mut vis = *vis;
            let mut next_parent = parent;
            loop {
                if vis != Visibility::Inherit {
                    break;
                }

                let Some(parent) = next_parent else {
                    break;
                };

                let Ok((parent_vis, parent)) = ancestors.get(parent.get()) else {
                    break;
                };

                vis = parent_vis.copied().unwrap_or_default();
                next_parent = parent;
            }

            let new_comp_vis = ComputedVisibility(match vis {
                Visibility::Inherit => true,
                Visibility::Visible => true,
                Visibility::Hidden => false,
            });

            if *comp_vis != new_comp_vis {
                *comp_vis = new_comp_vis;
            }
        });
}

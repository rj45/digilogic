pub use crux_core::Core;
use crux_core::{
    render::{render, Render},
    App, Command,
};
use serde::{Deserialize, Serialize};

use crate::model::Project;

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum Event {
    None,
    Reset,
    Increment,
    Decrement,
}

#[derive(Debug, Default)]
pub struct Model {
    pub project: Project,
    pub count: u32,
}

#[derive(Debug, Serialize, Deserialize, Clone, Default)]
pub struct ViewModel {
    pub data: String,
    pub count: u32,
}

#[cfg_attr(feature = "typegen", derive(crux_core::macros::Export))]
#[derive(crux_core::macros::Effect)]
#[allow(unused, missing_debug_implementations)]
pub struct Capabilities {
    render: Render<Event>,
}

#[derive(Default, Debug)]
pub struct Digilogic;

impl App for Digilogic {
    type Event = Event;
    type Model = Model;
    type ViewModel = ViewModel;
    type Capabilities = Capabilities;
    type Effect = Effect;

    fn update(
        &self,
        event: Self::Event,
        model: &mut Self::Model,
        _caps: &Self::Capabilities,
    ) -> Command<Effect, Event> {
        self.update(event, model)
    }

    fn view(&self, model: &Self::Model) -> Self::ViewModel {
        ViewModel {
            data: "Hello World".to_string(),
            count: model.count,
        }
    }
}

impl Digilogic {
    fn update(&self, event: Event, model: &mut Model) -> Command<Effect, Event> {
        match event {
            Event::None => {}
            Event::Reset => model.count = 0,
            Event::Increment => model.count += 1,
            Event::Decrement => model.count -= 1,
        };

        render()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hello_says_hello_world() {
        let hello = Digilogic::default();
        let mut model = Model {
            project: Project::default(),
            count: 0,
        };

        // Call 'update' and request effects
        let mut cmd = hello.update(Event::None, &mut model);

        // Check update asked us to `Render`
        cmd.expect_one_effect().expect_render();

        // Make sure the view matches our expectations
        let actual_view = &hello.view(&model).data;
        let expected_view = "Hello World";
        assert_eq!(actual_view, expected_view);
    }
}

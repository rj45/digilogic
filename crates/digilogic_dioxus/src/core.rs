use std::rc::Rc;

use digilogic_core2::core::{self, Digilogic, Effect, Event, ViewModel};
use dioxus::{
    prelude::{Signal, UnboundedReceiver},
    signals::Writable,
};
use futures_util::StreamExt;
use tracing::debug;

type Core = Rc<core::Core<Digilogic>>;

pub struct CoreService {
    core: Core,
    view: Signal<ViewModel>,
}

impl CoreService {
    pub fn new(view: Signal<ViewModel>) -> Self {
        debug!("initializing core service");
        Self {
            core: Rc::new(core::Core::new()),
            view,
        }
    }

    pub async fn run(&self, rx: &mut UnboundedReceiver<Event>) {
        let mut view = self.view;
        view.set(self.core.view());
        while let Some(event) = rx.next().await {
            self.update(event, &mut view);
        }
    }

    fn update(&self, event: Event, view: &mut Signal<ViewModel>) {
        debug!("event: {:?}", event);

        for effect in self.core.process_event(event) {
            process_effect(&self.core, effect, view);
        }
    }
}

fn process_effect(core: &Core, effect: Effect, view: &mut Signal<ViewModel>) {
    debug!("effect: {:?}", effect);

    match effect {
        Effect::Render(_) => {
            view.set(core.view());
        }
    };
}

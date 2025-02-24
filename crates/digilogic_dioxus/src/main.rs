#![allow(unused_qualifications, non_snake_case)]

mod core;

use dioxus::prelude::*;
use tracing::Level;

use digilogic_core2::core::{Event, ViewModel};

use core::CoreService;

use components::Hero;

mod components;

const FAVICON: Asset = asset!("/assets/favicon.ico");
const MAIN_CSS: Asset = asset!("/assets/styling/main.css");
const TAILWIND_CSS: Asset = asset!("/assets/tailwind.css");

#[component]
fn App() -> Element {
    let view = use_signal(ViewModel::default);

    let core = use_coroutine(move |mut rx| {
        let svc = CoreService::new(view);
        async move { svc.run(&mut rx).await }
    });
    rsx! {
        // Global app resources
        document::Link { rel: "icon", href: FAVICON }
        document::Link { rel: "stylesheet", href: MAIN_CSS }
        document::Link { rel: "stylesheet", href: TAILWIND_CSS }

        Hero {}


        main {
            section { class: "section has-text-centered",
                p { class: "is-size-5", "{view().count}" }
                div { class: "buttons section is-centered",
                    button {
                        class: "button is-primary is-danger",
                        onclick: move |_| {
                            core.send(Event::Reset);
                        },
                        "Reset"
                    }
                    button {
                        class: "button is-primary is-success",
                        onclick: move |_| {
                            core.send(Event::Increment);
                        },
                        "Increment"
                    }
                    button {
                        class: "button is-primary is-warning",
                        onclick: move |_| {
                            core.send(Event::Decrement);
                        },
                        "Decrement"
                    }
                }
            }
        }
    }
}

fn main() {
    dioxus_logger::init(Level::DEBUG).expect("failed to init logger");
    console_error_panic_hook::set_once();

    launch(App);
}

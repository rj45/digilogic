# Why Bevy ECS?

If you're not familiar with [Bevy ECS](https://docs.rs/bevy_ecs/latest/bevy_ecs/), you can there is a [gentle introduction](https://bevyengine.org/learn/quick-start/getting-started/ecs/) and a [more extensive guide](https://bevy-cheatbook.github.io/programming.html).

While building the C version of digilogic I slowly moved more towards an ECS system. But I never implemented the Systems part of ECS. But it became apparent that having Systems would really make the code a lot more modular and force chunks of logic to be less tightly coupled to each other.

One significant downside is that the code becomes extremely tightly coupled to Bevy ECS. It's basically the framework we are using throughout the app, and to move away from it would be a rewrite, or at least a very extensive refactor. It's also a lot of complexity to learn on top of the complexity of Rust.

But currently we believe that the benefits of Bevy ECS outweigh the drawbacks. We get much more modular code, a form of dependency injection, testability should we need it (though there aren't a lot of tests yet), a plugin system, a update loop manager with a way to schedule systems and express dependencies between those systems, and an amazing ecosystem of tooling built around it.

As well, Rust has a lot of complexity with lifetimes and such, and Bevy ECS allows you to (mostly) ignore lifetimes in systems, as that's handled for you by Bevy ECS.

So our hope is, once you learn Bevy ECS, it will be much easier to contribute to the code. Because it's modular, you can create a crate that has a plugin where you can put most of the functionality you want to add. You can create Components to add to entities to store the data you need, and Systems to contain the business logic.

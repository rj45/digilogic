#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_M {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x:literal,$y:literal $($tail:tt)*) => {
        $cur_x = $x as f64;
        $cur_y = $y as f64;
        $bez_path.move_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_m {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x:literal,$y:literal $($tail:tt)*) => {
        $cur_x += $x as f64;
        $cur_y += $y as f64;
        $bez_path.move_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_L {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x:literal,$y:literal $($tail:tt)*) => {
        $cur_x = $x as f64;
        $cur_y = $y as f64;
        $bez_path.line_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_l {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x:literal,$y:literal $($tail:tt)*) => {
        $cur_x += $x as f64;
        $cur_y += $y as f64;
        $bez_path.line_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_H {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x:literal $($tail:tt)*) => {
        $cur_x = $x as f64;
        $bez_path.line_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_h {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x:literal $($tail:tt)*) => {
        $cur_x += $x as f64;
        $bez_path.line_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_V {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $y:literal $($tail:tt)*) => {
        $cur_y = $y as f64;
        $bez_path.line_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_v {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $y:literal $($tail:tt)*) => {
        $cur_y += $y as f64;
        $bez_path.line_to(($cur_x, $cur_y));
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_C {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x1:literal,$y1:literal $x2:literal,$y2:literal $x3:literal,$y3:literal $($tail:tt)*) => {
        $cur_x = $x1 as f64;
        $cur_y = $y1 as f64;
        let p1 = ($cur_x, $cur_y);
        $cur_x = $x2 as f64;
        $cur_y = $y2 as f64;
        let p2 = ($cur_x, $cur_y);
        $cur_x = $x3 as f64;
        $cur_y = $y3 as f64;
        let p3 = ($cur_x, $cur_y);
        $bez_path.curve_to(p1, p2, p3);
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_c {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x1:literal,$y1:literal $x2:literal,$y2:literal $x3:literal,$y3:literal $($tail:tt)*) => {
        $cur_x += $x1 as f64;
        $cur_y += $y1 as f64;
        let p1 = ($cur_x, $cur_y);
        $cur_x += $x2 as f64;
        $cur_y += $y2 as f64;
        let p2 = ($cur_x, $cur_y);
        $cur_x += $x3 as f64;
        $cur_y += $y3 as f64;
        let p3 = ($cur_x, $cur_y);
        $bez_path.curve_to(p1, p2, p3);
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_Q {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x1:literal,$y1:literal $x2:literal,$y2:literal $($tail:tt)*) => {
        $cur_x = $x1 as f64;
        $cur_y = $y1 as f64;
        let p1 = ($cur_x, $cur_y);
        $cur_x = $x2 as f64;
        $cur_y = $y2 as f64;
        let p2 = ($cur_x, $cur_y);
        $bez_path.quad_to(p1, p2);
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
#[doc(hidden)]
macro_rules! bez_path_q {
    ($bez_path:ident, $cur_x:ident, $cur_y:ident;) => {};
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z) => {
        $bez_path.close_path();
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; z M $($tail:tt)*) => {
        $bez_path.close_path();
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; M $($tail:tt)*) => {
        $crate::ui::draw::bez_path_M!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; m $($tail:tt)*) => {
        $crate::ui::draw::bez_path_m!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; L $($tail:tt)*) => {
        $crate::ui::draw::bez_path_L!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; l $($tail:tt)*) => {
        $crate::ui::draw::bez_path_l!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; H $($tail:tt)*) => {
        $crate::ui::draw::bez_path_H!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; h $($tail:tt)*) => {
        $crate::ui::draw::bez_path_h!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; V $($tail:tt)*) => {
        $crate::ui::draw::bez_path_V!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; v $($tail:tt)*) => {
        $crate::ui::draw::bez_path_v!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; C $($tail:tt)*) => {
        $crate::ui::draw::bez_path_C!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; c $($tail:tt)*) => {
        $crate::ui::draw::bez_path_c!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; Q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_Q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; q $($tail:tt)*) => {
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
    ($bez_path:ident, $cur_x:ident, $cur_y:ident; $x1:literal,$y1:literal $x2:literal,$y2:literal $($tail:tt)*) => {
        $cur_x += $x1 as f64;
        $cur_y += $y1 as f64;
        let p1 = ($cur_x, $cur_y);
        $cur_x += $x2 as f64;
        $cur_y += $y2 as f64;
        let p2 = ($cur_x, $cur_y);
        $bez_path.quad_to(p1, p2);
        $crate::ui::draw::bez_path_q!($bez_path, $cur_x, $cur_y; $($tail)*);
    };
}

#[allow(unused_macros)]
macro_rules! bez_path {
    () => { vello::kurbo::BezPath::new() };
    (M $($tail:tt)*) => {{
        let mut bez_path = vello::kurbo::BezPath::new();
        let mut _cur_x = 0.0f64;
        let mut _cur_y = 0.0f64;
        $crate::ui::draw::bez_path_M!(bez_path, _cur_x, _cur_y; $($tail)*);
        bez_path
    }};
}

#[allow(unused_imports)]
pub(crate) use {
    bez_path, bez_path_C, bez_path_H, bez_path_L, bez_path_M, bez_path_Q, bez_path_V, bez_path_c,
    bez_path_h, bez_path_l, bez_path_m, bez_path_q, bez_path_v,
};

#[cfg(test)]
mod bez_path_test {
    use super::*;
    use vello::kurbo::PathEl::*;

    #[test]
    fn empty() {
        let p = bez_path!();
        assert_eq!(p.elements(), &[]);
    }

    #[test]
    fn close() {
        let p = bez_path!(M 1,1 Z);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            ClosePath,
        ]);
    }
    
    #[test]
    fn move_to() {
        let p = bez_path!(M 1,1 M 2,2);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            MoveTo((2.0, 2.0).into()),
        ]);
    }

    #[test]
    fn line_to() {
        let p = bez_path!(M 1,1 L 2,2);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            LineTo((2.0, 2.0).into()),
        ]);
    }

    #[test]
    fn hline_to() {
        let p = bez_path!(M 1,1 H 2);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            LineTo((2.0, 1.0).into()),
        ]);
    }
    
    #[test]
    fn vline_to() {
        let p = bez_path!(M 1,1 V 2);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            LineTo((1.0, 2.0).into()),
        ]);
    }

    #[test]
    fn curve_to() {
        let p = bez_path!(M 1,1 C 2,2 3,3 4,4);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            CurveTo((2.0, 2.0).into(), (3.0, 3.0).into(), (4.0, 4.0).into()),
        ]);
    }

    #[test]
    fn quad_to() {
        let p = bez_path!(M 1,1 Q 2,2 3,3);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            QuadTo((2.0, 2.0).into(), (3.0, 3.0).into()),
        ]);
    }
    
    #[test]
    fn move_to_rel() {
        let p = bez_path!(M 1,1 m 1,1);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            MoveTo((2.0, 2.0).into()),
        ]);
    }

    #[test]
    fn line_to_rel() {
        let p = bez_path!(M 1,1 l 1,1);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            LineTo((2.0, 2.0).into()),
        ]);
    }

    #[test]
    fn hline_to_rel() {
        let p = bez_path!(M 1,1 h 1);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            LineTo((2.0, 1.0).into()),
        ]);
    }
    
    #[test]
    fn vline_to_rel() {
        let p = bez_path!(M 1,1 v 1);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            LineTo((1.0, 2.0).into()),
        ]);
    }

    #[test]
    fn curve_to_rel() {
        let p = bez_path!(M 1,1 c 1,1 1,1 1,1);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            CurveTo((2.0, 2.0).into(), (3.0, 3.0).into(), (4.0, 4.0).into()),
        ]);
    }

    #[test]
    fn quad_to_rel() {
        let p = bez_path!(M 1,1 q 1,1 1,1);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            QuadTo((2.0, 2.0).into(), (3.0, 3.0).into()),
        ]);
    }

    #[test]
    fn combined() {
        let p = bez_path!(M 3,7 H 4.4 C 6.7,7 7.7,6.9 9,4 7.7,1.1 6.7,1 4.4,1 H 3 C 4.4,3.1 4.4,4.9 3,7 Z);
        assert_eq!(p.elements(), &[
            MoveTo((3.0, 7.0).into()),
            LineTo((4.4, 7.0).into()),
            CurveTo((6.7, 7.0).into(), (7.7, 6.9).into(), (9.0, 4.0).into()),
            CurveTo((7.7, 1.1).into(), (6.7, 1.0).into(), (4.4, 1.0).into()),
            LineTo((3.0, 1.0).into()),
            CurveTo((4.4, 3.1).into(), (4.4, 4.9).into(), (3.0, 7.0).into()),
            ClosePath,
        ]);
    }

    #[test]
    fn continue_after_close() {
        let p = bez_path!(M 1,1 Z M 2,2);
        assert_eq!(p.elements(), &[
            MoveTo((1.0, 1.0).into()),
            ClosePath,
            MoveTo((2.0, 2.0).into()),
        ]);
    }
}

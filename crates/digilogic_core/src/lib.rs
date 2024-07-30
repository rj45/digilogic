pub mod bundles;
pub mod components;
pub mod events;
pub mod symbol;
pub mod transform;

#[macro_use]
extern crate static_assertions;

use std::borrow::Borrow;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::ops::Deref;
use std::sync::Arc;

const SHARED_STR_INLINE_CAP: usize =
    (std::mem::size_of::<usize>() * 3) - (std::mem::size_of::<u8>() * 2);
const_assert!(SHARED_STR_INLINE_CAP <= (u8::MAX as usize));

enum SharedStrRepr {
    Static(&'static str),
    Arc(Arc<str>),
    Small {
        len: u8,
        data: [u8; SHARED_STR_INLINE_CAP],
    },
}

#[repr(transparent)]
pub struct SharedStr(SharedStrRepr);

impl Deref for SharedStr {
    type Target = str;

    #[inline]
    fn deref(&self) -> &Self::Target {
        match self.0 {
            SharedStrRepr::Static(s) => s,
            SharedStrRepr::Arc(ref s) => s,
            SharedStrRepr::Small { len, ref data } => {
                #[allow(unsafe_code)]
                unsafe {
                    // SAFETY: `len` and `data` originate from a valid string slice
                    std::str::from_utf8_unchecked(&data[..(len as usize)])
                }
            }
        }
    }
}

impl AsRef<str> for SharedStr {
    #[inline]
    fn as_ref(&self) -> &str {
        self.deref()
    }
}

impl Borrow<str> for SharedStr {
    #[inline]
    fn borrow(&self) -> &str {
        self.deref()
    }
}

impl Clone for SharedStr {
    fn clone(&self) -> Self {
        match self.0 {
            SharedStrRepr::Static(s) => Self(SharedStrRepr::Static(s)),
            SharedStrRepr::Arc(ref s) => Self(SharedStrRepr::Arc(Arc::clone(s))),
            SharedStrRepr::Small { len, data } => Self(SharedStrRepr::Small { len, data }),
        }
    }
}

impl PartialEq for SharedStr {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.deref() == other.deref()
    }
}

impl PartialEq<str> for SharedStr {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        self.deref() == other
    }
}

impl PartialEq<SharedStr> for str {
    #[inline]
    fn eq(&self, other: &SharedStr) -> bool {
        self == other.deref()
    }
}

impl Eq for SharedStr {}

impl Hash for SharedStr {
    #[inline]
    fn hash<H: Hasher>(&self, state: &mut H) {
        Hash::hash(self.deref(), state)
    }
}

impl SharedStr {
    #[inline]
    pub const fn new_static(s: &'static str) -> Self {
        Self(SharedStrRepr::Static(s))
    }

    #[inline]
    fn new_small(s: &str) -> Self {
        assert!(s.len() <= SHARED_STR_INLINE_CAP);

        let mut data = [0u8; SHARED_STR_INLINE_CAP];
        data[..s.len()].copy_from_slice(s.as_bytes());

        Self(SharedStrRepr::Small {
            len: s.len() as u8,
            data,
        })
    }
}

impl From<&str> for SharedStr {
    fn from(s: &str) -> Self {
        if s.len() <= SHARED_STR_INLINE_CAP {
            Self::new_small(s)
        } else {
            Self(SharedStrRepr::Arc(s.into()))
        }
    }
}

impl From<String> for SharedStr {
    fn from(s: String) -> Self {
        if s.len() <= SHARED_STR_INLINE_CAP {
            Self::new_small(&s)
        } else {
            Self(SharedStrRepr::Arc(s.into()))
        }
    }
}

impl From<Box<str>> for SharedStr {
    fn from(s: Box<str>) -> Self {
        if s.len() <= SHARED_STR_INLINE_CAP {
            Self::new_small(&s)
        } else {
            Self(SharedStrRepr::Arc(s.into()))
        }
    }
}

impl From<Arc<str>> for SharedStr {
    #[inline]
    fn from(s: Arc<str>) -> Self {
        Self(SharedStrRepr::Arc(s))
    }
}

impl fmt::Debug for SharedStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(self.deref(), f)
    }
}

impl fmt::Display for SharedStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(self.deref(), f)
    }
}

impl serde::Serialize for SharedStr {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(self)
    }
}

struct SharedStrVisitor;

impl<'de> serde::de::Visitor<'de> for SharedStrVisitor {
    type Value = String;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a string")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Ok(v.to_owned())
    }

    fn visit_string<E>(self, v: String) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Ok(v)
    }

    fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        match std::str::from_utf8(v) {
            Ok(s) => Ok(s.to_owned()),
            Err(_) => Err(serde::de::Error::invalid_value(
                serde::de::Unexpected::Bytes(v),
                &self,
            )),
        }
    }

    fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        match String::from_utf8(v) {
            Ok(s) => Ok(s),
            Err(e) => Err(serde::de::Error::invalid_value(
                serde::de::Unexpected::Bytes(&e.into_bytes()),
                &self,
            )),
        }
    }
}

impl<'de> serde::Deserialize<'de> for SharedStr {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer
            .deserialize_string(SharedStrVisitor)
            .map(Into::into)
    }
}

#[derive(Default)]
pub struct CorePlugin;

impl bevy_app::Plugin for CorePlugin {
    fn build(&self, app: &mut bevy_app::App) {
        app.init_resource::<symbol::SymbolRegistry>();
        app.add_event::<events::LoadEvent>();
        app.add_event::<events::LoadedEvent>();
        app.add_systems(bevy_app::PostUpdate, transform::update_transforms);
    }
}

use std::collections::HashMap;
use std::path::Path;
use serde::{Serialize, Deserialize};
use serde_json::Value;

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CircuitFile {
    pub version: u32,
    pub modules: Vec<Module>
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Module {
    pub id: String,
    pub name: String,
    pub prefix: String,
    #[serde(rename = "symbolKind")]
    pub symbol_kind: String,
    pub symbols: Vec<Symbol>,
    pub nets: Vec<Net>
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Symbol {
    pub id: String,
    pub position: Vec<f64>,
    pub number: u32
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Net {
    pub id: String,
    pub name: String,
    pub subnets: Vec<Subnet>
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Subnet {
    pub id: String,
    pub name: String,
    #[serde(rename = "subnetBits")]
    pub subnet_bits: Vec<Value>,
    pub endpoints: Vec<Endpoint>
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Endpoint {
    pub id: String,
    pub position: Vec<f64>,
    pub portref: HashMap<String, String>,
    pub waypoints: Vec<Value>
}

impl TryFrom<&str> for CircuitFile {
    type Error = serde_json::Error;

    #[inline]
    fn try_from(value: &str) -> Result<Self, Self::Error> {
        serde_json::from_str(value)
    }    
}

impl CircuitFile {
    pub fn load<P: AsRef<Path>>(path: P) -> anyhow::Result<Self> {
        let file = std::fs::File::open(path)?;
        let reader = std::io::BufReader::new(file);
        Ok(serde_json::from_reader(reader)?)
    }
}

#[cfg(test)]
mod tests {
    use super::CircuitFile;

    #[test]
    fn reads_small_sample() {
        CircuitFile::load("testdata/small.dlc").unwrap();
    }

    #[test]
    fn reads_medium_sample() {
        CircuitFile::load("testdata/medium.dlc").unwrap();
    }

    #[test]
    fn reads_large_sample() {
        CircuitFile::load("testdata/large.dlc").unwrap();
    }
}

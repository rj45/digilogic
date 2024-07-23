use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(PartialEq, Eq, Hash, Debug, Serialize, Deserialize, Clone)]
pub struct Id(pub String);

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CircuitFile {
    pub version: u32,
    pub modules: Vec<Module>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Module {
    pub id: Id,
    pub name: String,
    pub prefix: String,
    #[serde(rename = "symbolKind")]
    pub symbol_kind: Id,
    pub symbols: Vec<Symbol>,
    pub nets: Vec<Net>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Symbol {
    pub id: Id,
    #[serde(rename = "symbolKindName")]
    pub symbol_kind_name: Option<String>,
    #[serde(rename = "symbolKindID")]
    pub symbol_kind_id: Option<Id>,
    pub position: [f32; 2],
    pub number: u32,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Net {
    pub id: Id,
    pub name: String,
    pub subnets: Vec<Subnet>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Subnet {
    pub id: Id,
    pub name: String,
    #[serde(rename = "subnetBits")]
    pub subnet_bits: Vec<serde_json::Value>,
    pub endpoints: Vec<Endpoint>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct PortRef {
    pub symbol: Id,
    #[serde(rename = "portName")]
    pub port_name: Option<String>,
    pub port: Option<Id>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Waypoint {
    pub id: Id,
    pub position: [f32; 2],
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Endpoint {
    pub id: Id,
    pub position: [f32; 2],
    pub portref: PortRef,
    pub waypoints: Vec<Waypoint>,
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

    pub fn save<P: AsRef<Path>>(&self, path: P) -> anyhow::Result<()> {
        let file = std::fs::File::create(path)?;
        let writer = std::io::BufWriter::new(file);
        serde_json::to_writer_pretty(writer, self)?;
        Ok(())
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

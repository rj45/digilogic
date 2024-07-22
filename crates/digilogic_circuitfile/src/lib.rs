use std::{collections::HashMap, fs::File, io::Read, path::PathBuf};

use anyhow::Result;
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
    type Error = anyhow::Error;

    fn try_from(value: &str) -> Result<Self> {
        Ok(serde_json::from_str(value)?)
    }    
}

impl CircuitFile {
    pub fn load(path: PathBuf) -> Result<Self> {
        let mut file = File::open(path)?;
        let mut contents = String::new();
        file.read_to_string(&mut contents)?;
        Ok(Self::try_from(contents.as_str())?)
    }
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;
    use anyhow::Result;
    use crate::CircuitFile;

    fn get_sample_path(sample_file: &str) -> PathBuf {
        let cwd = std::env::current_dir().expect("Could not determine current working directory");
        cwd.join("assets/testdata").join(sample_file)
    }

    #[test]
    fn reads_small_sample() -> Result<()> {
        let file_path = get_sample_path("small.dlc");
        let circuit_file = CircuitFile::load(file_path.into());
        assert!(circuit_file.is_ok());
        Ok(())
    }

    #[test]
    fn reads_medium_sample() -> Result<()> {
        let file_path = get_sample_path("medium.dlc");
        let circuit_file = CircuitFile::load(file_path.into());
        assert!(circuit_file.is_ok());
        Ok(())
    }

    #[test]
    fn reads_large_sample() -> Result<()> {
        let file_path = get_sample_path("large.dlc");
        let circuit_file = CircuitFile::load(file_path.into());
        assert!(circuit_file.is_ok());
        Ok(())
    }
}

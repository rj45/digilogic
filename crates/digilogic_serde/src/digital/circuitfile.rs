use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct Circuit {
    pub version: String,
    pub attributes: Attributes,
    pub visual_elements: VisualElements,
    pub wires: Wires,
    pub measurement_ordering: MeasurementOrdering,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Attributes {
    pub entry: Option<Vec<AttributesEntry>>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AttributesEntry {
    #[serde(rename = "$value")]
    pub value: [AttributeValue; 2],
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub enum AttributeValue {
    #[serde(rename = "awt-color")]
    AwtColor(AwtColor),
    Data(String),
    File(String),
    TestData(TestData),
    Value(Value),
    InverterConfig(InverterConfig),
    IntFormat(String),
    Long(i64),
    Int(i32),
    Boolean(String),
    Rotation(Rotation),
    String(String),
    ShapeType(String),
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VisualElements {
    #[serde(rename = "visualElement")]
    pub visual_element: Vec<VisualElement>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct VisualElement {
    pub element_name: String,
    pub element_attributes: Attributes,
    pub pos: Point,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AwtColor {
    pub red: u8,
    pub green: u8,
    pub blue: u8,
    pub alpha: u8,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TestData {
    #[serde(rename = "dataString")]
    pub data_string: String,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Value {
    pub v: String,
    pub z: String,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct InverterConfig {
    pub string: Option<Vec<String>>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Rotation {
    pub rotation: String,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Point {
    pub x: i32,
    pub y: i32,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Wires {
    pub wire: Vec<Wire>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Wire {
    pub p1: Point,
    pub p2: Point,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct MeasurementOrdering {
    pub string: Option<Vec<String>>,
}

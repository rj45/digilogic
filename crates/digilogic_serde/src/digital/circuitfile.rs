use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Circuit {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub version: String,
    pub attributes: Attributes,
    #[serde(rename = "visualElements")]
    pub visual_elements: VisualElements,
    pub wires: Wires,
    #[serde(rename = "measurementOrdering")]
    pub measurement_ordering: MeasurementOrdering,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Attributes {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub entry: Vec<AttributesEntry>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AttributesEntry {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub file: Option<String>,
    pub boolean: Option<String>,
    #[serde(rename = "shapeType")]
    pub shape_type: Option<String>,
    #[serde(rename = "awt-color")]
    pub awt_color: Option<AttributesEntryAwtColor>,
    pub string: Vec<String>,
    pub int: Option<String>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AttributesEntryAwtColor {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub red: String,
    pub green: String,
    pub blue: String,
    pub alpha: String,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VisualElements {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    #[serde(rename = "visualElement")]
    pub visual_element: Vec<VisualElement>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct VisualElement {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    #[serde(rename = "elementName")]
    pub element_name: String,
    #[serde(rename = "elementAttributes")]
    pub element_attributes: ElementAttributes,
    pub pos: Point,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ElementAttributes {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub entry: Vec<ElementAttributesEntry>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ElementAttributesEntry {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    #[serde(rename = "awt-color")]
    pub awt_color: Option<ElementAttributesEntryAwtColor>,
    pub data: Option<String>,
    pub file: Option<String>,
    #[serde(rename = "testData")]
    pub test_data: Option<TestData>,
    pub value: Option<Value>,
    #[serde(rename = "inverterConfig")]
    pub inverter_config: Option<InverterConfig>,
    #[serde(rename = "intFormat")]
    pub int_format: Option<String>,
    pub long: Option<String>,
    pub int: Option<String>,
    pub boolean: Option<String>,
    pub rotation: Option<Rotation>,
    pub string: Vec<String>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ElementAttributesEntryAwtColor {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub red: String,
    pub green: String,
    pub blue: String,
    pub alpha: String,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct TestData {
    #[serde(rename = "$text")]
    pub text: Option<String>,
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
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub string: Vec<String>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Rotation {
    pub rotation: String,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Point {
    pub x: u32,
    pub y: u32,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Wires {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub wire: Vec<Wire>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Wire {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub p1: Point,
    pub p2: Point,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct MeasurementOrdering {
    #[serde(rename = "$text")]
    pub text: Option<String>,
    pub string: Vec<String>,
}

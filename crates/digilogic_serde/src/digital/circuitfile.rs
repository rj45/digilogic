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
    pub entry: Vec<AttributesEntry>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AttributesEntry {
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
    pub red: String,
    pub green: String,
    pub blue: String,
    pub alpha: String,
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
    pub element_attributes: ElementAttributes,
    pub pos: Point,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ElementAttributes {
    pub entry: Vec<ElementAttributesEntry>,
}

#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ElementAttributesEntry {
    #[serde(rename = "awt-color")]
    pub awt_color: Option<ElementAttributesEntryAwtColor>,
    pub data: Option<String>,
    pub file: Option<String>,
    pub test_data: Option<TestData>,
    pub value: Option<Value>,
    pub inverter_config: Option<InverterConfig>,
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
    pub red: String,
    pub green: String,
    pub blue: String,
    pub alpha: String,
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
    pub string: Vec<String>,
}

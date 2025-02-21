use std::sync::Arc;

use super::*;
use crate::intern::Intern;
use crate::table::Id;

#[derive(Debug, Default)]
pub struct ProjectBuilder {
    project: Project,
}

impl ProjectBuilder {
    pub fn from(project: Project) -> Self {
        Self { project }
    }

    pub fn build(self) -> Project {
        self.project
    }
}

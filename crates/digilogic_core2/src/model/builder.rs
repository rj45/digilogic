use super::*;

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

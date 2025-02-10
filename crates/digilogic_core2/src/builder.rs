use crate::Project;

#[derive(Debug)]
pub struct ProjectBuilder<'a> {
    project: &'a mut Project,
}

impl<'a> ProjectBuilder<'a> {
    pub fn from(project: &'a mut Project) -> Self {
        Self { project }
    }
}

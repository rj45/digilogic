use crate::Project;

mod stage1;
mod stage2;
mod stage3;

pub fn import_into(reader: impl std::io::Read, project: &mut Project) -> anyhow::Result<()> {
    stage3::Translator::import_into(reader, project)
}

pub fn import(reader: impl std::io::Read) -> anyhow::Result<Project> {
    let mut project = Project::default();
    import_into(reader, &mut project)?;
    Ok(project)
}

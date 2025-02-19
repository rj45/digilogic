use crate::db::Project;

mod stage1;
mod stage2;
mod stage3;

pub fn import_into(reader: impl std::io::Read, project: Project) -> anyhow::Result<Project> {
    stage3::Importer::import_into(reader, project)
}

pub fn import(reader: impl std::io::Read) -> anyhow::Result<Project> {
    import_into(reader, Project::default())
}

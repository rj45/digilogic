use super::stage2;

pub struct Translator<'a> {
    interm: stage2::Project,
    builder: crate::ProjectBuilder<'a>,
}

impl<'a> Translator<'a> {
    pub fn import_into(
        reader: impl std::io::Read,
        project: &'a mut crate::Project,
    ) -> anyhow::Result<()> {
        let stage2 = stage2::Importer::import(reader)?;
        let builder = crate::ProjectBuilder::from(project);
        let translator = Translator {
            interm: stage2,
            builder,
        };
        translator.translate()
    }

    fn translate(self) -> anyhow::Result<()> {
        Ok(())
    }
}

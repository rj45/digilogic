use super::*;

pub trait ForeignKey<T> {
    fn foreign_key(&self) -> Id<T>;
}

#[derive(Debug)]
pub struct ReverseIndex<L: ForeignKey<F>, F> {
    ranges: SecondaryTable<F, (u32, u32)>,
    ids: Vec<Id<L>>,
}

impl<L: ForeignKey<F>, F> ReverseIndex<L, F> {
    pub fn new(primary_table: &Table<L>, foreign_table: &Table<F>) -> Result<Self, Error> {
        let mut ranges = SecondaryTable::default();
        let mut ids = Vec::new();
        for (fid, _) in foreign_table.iter() {
            let start = ids.len() as u32;
            ids.extend(primary_table.iter().filter_map(|(lid, fk)| {
                if fk.foreign_key() == fid {
                    Some(lid)
                } else {
                    None
                }
            }));
            let end = ids.len() as u32;
            ranges.insert_with_id(fid, (start, end))?;
        }
        Ok(Self { ranges, ids })
    }

    pub fn get(&self, fid: Id<F>) -> &[Id<L>] {
        let (start, end) = self.ranges[fid];
        &self.ids[start as usize..end as usize]
    }
}

use smallvec::SmallVec;

use super::*;

pub trait Child<P> {
    fn parent(&self) -> Id<P>;
}

/// A trait for indexing 1:N (Parent:Child) relationships between tables.
/// There are two implementations of this index:
/// - DenseChildIndex: stores child ids densely in a vector which is more
///   cache friendly but slower to update.
/// - SmallVecChildIndex: stores children in SmallVecs, which is less
///   cache friendly but fast to update.
pub trait ChildIndex<C: Child<P>, P>: Default {
    fn get(&self, fid: Id<P>) -> Option<&[Id<C>]>;

    /// A new entry in the parent table has been inserted. This presumes
    /// there are no existing children for this parent.
    fn insert_parent(&mut self, pid: Id<P>) -> Result<(), Error>;

    /// An entry in the parent table has been removed.
    fn remove_parent(&mut self, pid: Id<P>) -> Result<(), Error>;

    /// A new entry in the child table has been inserted. The parent is
    /// expected to exist.
    fn insert_child(&mut self, cid: Id<C>, child: &Table<C>) -> Result<(), Error>;

    /// An entry in the child table has been removed.
    fn remove_child(&mut self, cid: Id<C>, child: &Table<C>) -> Result<(), Error>;

    /// Rebuild the index from scratch.
    fn rebuild(&mut self, child: &Table<C>, parent: &Table<P>);

    /// Verify the integrity of the index.
    /// This is a slow operation and should only be used for debugging.
    /// It will panic if the index is invalid.
    fn verify(&self, child: &Table<C>, parent: &Table<P>);
}

/// An index for 1:N relationships that maps N children to 1 parent.
/// Child Ids are stored densely in a vector and is suitable for relations
/// that do not change often, since inserting/removing children is expensive.
#[derive(Debug)]
pub struct DenseChildIndex<C: Child<P>, P> {
    index: SecondaryTable<P, u32>,
    range: Vec<(u32, u32)>,
    child_ids: Vec<Id<C>>,
}

impl<C: Child<P>, P> Default for DenseChildIndex<C, P> {
    fn default() -> Self {
        Self {
            index: SecondaryTable::default(),
            range: Vec::new(),
            child_ids: Vec::new(),
        }
    }
}

impl<C: Child<P>, P> ChildIndex<C, P> for DenseChildIndex<C, P> {
    fn get(&self, fid: Id<P>) -> Option<&[Id<C>]> {
        let index = self.index.get(&fid)?;
        let (start, end) = self.range[*index as usize];
        Some(&self.child_ids[start as usize..end as usize])
    }

    /// A new entry in the parent table has been inserted. This presumes
    /// there are no existing children for this parent.
    fn insert_parent(&mut self, pid: Id<P>) -> Result<(), Error> {
        let index = self.range.len() as u32;
        let end = self.child_ids.len() as u32;
        self.range.push((end, end));
        self.index.insert_with_id(pid, index)
    }

    /// An entry in the parent table has been removed.
    fn remove_parent(&mut self, pid: Id<P>) -> Result<(), Error> {
        let index = self.index.remove(&pid).ok_or(Error::InvalidId)?;
        for i in self.index.values_mut() {
            if *i > index {
                *i -= 1;
            }
        }
        let (start, end) = self.range.remove(index as usize);
        // remove the child ids
        self.child_ids.drain(start as usize..end as usize);
        Ok(())
    }

    /// A new entry in the child table has been inserted. The parent is
    /// expected to exist.
    fn insert_child(&mut self, cid: Id<C>, child: &Table<C>) -> Result<(), Error> {
        let pid = child[cid].parent();
        let index = self.index.get(&pid).ok_or(Error::InvalidId)?;
        let (_, end) = self
            .range
            .get_mut(*index as usize)
            .ok_or(Error::InvalidId)?;
        if *end as usize > self.child_ids.len() {
            self.child_ids.push(cid);
        } else {
            self.child_ids.insert(*end as usize, cid);
        }
        *end += 1;
        for i in *index as usize + 1..self.range.len() {
            let (rstart, rend) = &mut self.range[i];
            *rstart += 1;
            *rend += 1;
        }
        Ok(())
    }

    /// An entry in the child table has been removed.
    fn remove_child(&mut self, cid: Id<C>, child: &Table<C>) -> Result<(), Error> {
        let pid = child[cid].parent();
        let index = self.index.get(&pid).ok_or(Error::InvalidId)?;
        let (start, end) = self
            .range
            .get_mut(*index as usize)
            .ok_or(Error::InvalidId)?;
        for idx in *start as usize..*end as usize {
            if self.child_ids[idx] == cid {
                self.child_ids.remove(idx);
                break;
            }
        }
        *end -= 1;
        for i in *index as usize + 1..self.range.len() {
            let (rstart, rend) = &mut self.range[i];
            *rstart -= 1;
            *rend -= 1;
        }
        Ok(())
    }

    /// Rebuild the index from scratch.
    fn rebuild(&mut self, child: &Table<C>, parent: &Table<P>) {
        self.index.clear();
        self.range.clear();
        self.child_ids.clear();
        for (pid, _) in parent.iter() {
            self.insert_parent(pid).expect("unreachable");
        }
        for (cid, _) in child.iter() {
            self.insert_child(cid, child).expect("unreachable");
        }
    }

    /// Verify the integrity of the index.
    /// This is a slow operation and should only be used for debugging.
    /// It will panic if the index is invalid.
    fn verify(&self, child: &Table<C>, parent: &Table<P>) {
        for pid in parent.keys() {
            let index = self.index.get(&pid).expect("Missing parent");
            let (start, end) = self.range[*index as usize];
            for i in start..end {
                let cid = self.child_ids[i as usize];
                if child[cid].parent() != pid {
                    panic!("Invalid foreign key");
                }
            }
        }
        for (cid, child) in child.iter() {
            let pid = child.parent();
            let index = self.index.get(&pid).expect("Missing parent");
            let (start, end) = self.range[*index as usize];
            if !self.child_ids[start as usize..end as usize].contains(&cid) {
                panic!("Missing child");
            }
        }
        for (pid, index) in self.index.iter() {
            let (start, end) = self.range[*index as usize];
            if parent.get(&pid).is_none() {
                panic!("Missing parent");
            }
            for i in start..end {
                let cid = self.child_ids[i as usize];
                if child[cid].parent() != pid {
                    panic!("Invalid foreign key");
                }
            }
        }
    }
}

/// An index for 1:N relationships that maps N children to 1 parent.
/// Child Ids are stored sparsely in a SmallVec and is suitable for relations
/// that change often, since inserting/removing children is cheap.
#[derive(Debug)]
pub struct SmallVecChildIndex<C: Child<P>, P> {
    children: SecondaryTable<P, SmallVec<[Id<C>; 4]>>,
}

impl<C: Child<P>, P> Default for SmallVecChildIndex<C, P> {
    fn default() -> Self {
        Self {
            children: SecondaryTable::default(),
        }
    }
}

impl<C: Child<P>, P> ChildIndex<C, P> for SmallVecChildIndex<C, P> {
    fn get(&self, fid: Id<P>) -> Option<&[Id<C>]> {
        self.children.get(&fid).map(|v| &v[..])
    }

    fn insert_parent(&mut self, pid: Id<P>) -> Result<(), Error> {
        self.children.insert_with_id(pid, SmallVec::new())
    }

    fn remove_parent(&mut self, pid: Id<P>) -> Result<(), Error> {
        self.children.remove(&pid);
        Ok(())
    }

    fn insert_child(&mut self, cid: Id<C>, child: &Table<C>) -> Result<(), Error> {
        let pid = child[cid].parent();
        self.children
            .get_mut(&pid)
            .ok_or(Error::InvalidId)?
            .push(cid);
        Ok(())
    }

    fn remove_child(&mut self, cid: Id<C>, child: &Table<C>) -> Result<(), Error> {
        let pid = child[cid].parent();
        let children = self.children.get_mut(&pid).ok_or(Error::InvalidId)?;
        let idx = children
            .iter()
            .position(|&id| id == cid)
            .ok_or(Error::InvalidId)?;
        children.remove(idx);
        Ok(())
    }

    fn rebuild(&mut self, child: &Table<C>, parent: &Table<P>) {
        self.children.clear();
        for (fid, _) in parent.iter() {
            self.children
                .insert_with_id(fid, SmallVec::new())
                .expect("unreachable");
        }
        for (cid, child) in child.iter() {
            let pid = child.parent();
            self.children.get_mut(&pid).unwrap().push(cid);
        }
    }

    fn verify(&self, child: &Table<C>, parent: &Table<P>) {
        for (pid, children) in self.children.iter() {
            if !parent.contains_key(&pid) {
                panic!("Missing parent");
            }
            for &cid in children {
                if child[cid].parent() != pid {
                    panic!("Invalid foreign key");
                }
            }
        }
        for (cid, child) in child.iter() {
            let pid = child.parent();
            if !parent.contains_key(&pid) {
                panic!("Missing parent");
            }
            if !self.children[pid].contains(&cid) {
                panic!("Missing child");
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[derive(Debug, Default)]
    struct Parent;

    #[derive(Debug)]
    struct Child1 {
        parent: Id<Parent>,
    }

    impl Child<Parent> for Child1 {
        fn parent(&self) -> Id<Parent> {
            self.parent
        }
    }

    fn test_child_index<CI: ChildIndex<Child1, Parent>>(
        parent_table: &mut Table<Parent>,
        child_table: &mut Table<Child1>,
        mut index: CI,
    ) {
        let p1 = parent_table.insert(Parent);
        let p2 = parent_table.insert(Parent);
        let p3 = parent_table.insert(Parent);

        let c1 = child_table.insert(Child1 { parent: p1 });
        let c2 = child_table.insert(Child1 { parent: p1 });
        let c3 = child_table.insert(Child1 { parent: p2 });
        let c4 = child_table.insert(Child1 { parent: p2 });
        let c5 = child_table.insert(Child1 { parent: p3 });

        index.insert_parent(p1).unwrap();
        index.insert_parent(p2).unwrap();
        index.insert_parent(p3).unwrap();

        index.insert_child(c1, child_table).unwrap();
        index.insert_child(c2, child_table).unwrap();
        index.insert_child(c3, child_table).unwrap();
        index.insert_child(c4, child_table).unwrap();
        index.insert_child(c5, child_table).unwrap();

        index.verify(child_table, parent_table);

        index.remove_child(c1, child_table).unwrap();
        index.remove_child(c2, child_table).unwrap();
        index.remove_child(c3, child_table).unwrap();
        index.remove_child(c4, child_table).unwrap();
        index.remove_child(c5, child_table).unwrap();

        child_table.remove(&c1);
        child_table.remove(&c2);
        child_table.remove(&c3);
        child_table.remove(&c4);
        child_table.remove(&c5);

        index.verify(child_table, parent_table);

        index.remove_parent(p1).unwrap();
        index.remove_parent(p2).unwrap();
        index.remove_parent(p3).unwrap();

        parent_table.remove(&p1);
        parent_table.remove(&p2);
        parent_table.remove(&p3);

        index.verify(child_table, parent_table);
    }

    #[test]
    fn test_dense_child_index() {
        let mut parent_table = Table::default();
        let mut child_table = Table::default();
        let index = DenseChildIndex::default();
        test_child_index(&mut parent_table, &mut child_table, index);
    }

    #[test]
    fn test_smallvec_child_index() {
        let mut parent_table = Table::default();
        let mut child_table = Table::default();
        let index = SmallVecChildIndex::default();
        test_child_index(&mut parent_table, &mut child_table, index);
    }
}

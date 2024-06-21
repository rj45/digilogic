#include "core/core.h"
#include "utest.h"

#include "ux.h"

UTEST(CircuitUX, init) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ux_free(&ux);
}

UTEST(CircuitUX, adding_components) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit2, "AND");
  ID orSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit2, "OR");

  ux_start_adding_symbol(&ux, andSymbolKindID);

  ASSERT_TRUE(circ_has(&ux.view.circuit2, ux.addingSymbol));
  ASSERT_EQ(
    circ_get(&ux.view.circuit2, ux.addingSymbol, SymbolKindID),
    andSymbolKindID);

  ID oldID = ux.addingSymbol;

  ux_change_adding_symbol(&ux, orSymbolKindID);

  ASSERT_FALSE(circ_has(&ux.view.circuit2, oldID));
  ASSERT_TRUE(circ_has(&ux.view.circuit2, ux.addingSymbol));
  ASSERT_EQ(
    circ_get(&ux.view.circuit2, ux.addingSymbol, SymbolKindID), orSymbolKindID);

  oldID = ux.addingSymbol;

  ux_stop_adding_symbol(&ux);

  ASSERT_FALSE(circ_has(&ux.view.circuit2, oldID));
  ASSERT_EQ(ux.addingSymbol, NO_ID);

  ux_free(&ux);
}

UTEST(CircuitUX, selection_and_deselection) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit2, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit2, ux.view.circuit2.top, andSymbolKindID);
  HMM_Vec2 position = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit2, symbolID, position);

  // Test selection
  UndoCommand selectCmd = undo_cmd_select_item(symbolID);
  ux_do(&ux, selectCmd);
  ASSERT_EQ(arrlen(ux.view.selected), 1);
  ASSERT_EQ(ux.view.selected[0], symbolID);

  // Test deselection
  UndoCommand deselectCmd = undo_cmd_deselect_item(symbolID);
  ux_do(&ux, deselectCmd);
  ASSERT_EQ(arrlen(ux.view.selected), 0);

  ux_free(&ux);
}

UTEST(CircuitUX, move_component) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit2, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit2, ux.view.circuit2.top, andSymbolKindID);
  HMM_Vec2 initialPosition = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit2, symbolID, initialPosition);

  // Select the symbol
  UndoCommand selectCmd = undo_cmd_select_item(symbolID);
  ux_do(&ux, selectCmd);

  // Move the symbol
  HMM_Vec2 newPosition = HMM_V2(200, 200);
  UndoCommand moveCmd =
    undo_cmd_move_selection(initialPosition, newPosition, false);
  ux_do(&ux, moveCmd);

  Position finalPosition = circ_get(&ux.view.circuit2, symbolID, Position);
  ASSERT_EQ(finalPosition.X, newPosition.X);
  ASSERT_EQ(finalPosition.Y, newPosition.Y);

  ux_free(&ux);
}

UTEST(CircuitUX, delete_component) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit2, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit2, ux.view.circuit2.top, andSymbolKindID);
  HMM_Vec2 position = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit2, symbolID, position);

  // Select the symbol
  UndoCommand selectCmd = undo_cmd_select_item(symbolID);
  ux_do(&ux, selectCmd);

  // Delete the symbol
  UndoCommand deleteCmd =
    undo_cmd_del_symbol(position, symbolID, andSymbolKindID);
  ux_do(&ux, deleteCmd);

  ASSERT_FALSE(circ_has(&ux.view.circuit2, symbolID));

  ux_free(&ux);
}

UTEST(CircuitUX, undo_redo) {
  CircuitUX ux;
  ux_init(&ux, circuit_component_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit2, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit2, ux.view.circuit2.top, andSymbolKindID);
  HMM_Vec2 position = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit2, symbolID, position);

  // Select the symbol
  UndoCommand selectCmd = undo_cmd_select_item(symbolID);
  ux_do(&ux, selectCmd);

  // Move the symbol
  HMM_Vec2 newPosition = HMM_V2(200, 200);
  UndoCommand moveCmd = undo_cmd_move_selection(position, newPosition, false);
  ux_do(&ux, moveCmd);

  // Undo the move
  ux_undo(&ux);
  Position undoPosition = circ_get(&ux.view.circuit2, symbolID, Position);
  ASSERT_EQ(undoPosition.X, position.X);
  ASSERT_EQ(undoPosition.Y, position.Y);

  // Redo the move
  ux_redo(&ux);
  Position redoPosition = circ_get(&ux.view.circuit2, symbolID, Position);
  ASSERT_EQ(redoPosition.X, newPosition.X);
  ASSERT_EQ(redoPosition.Y, newPosition.Y);

  ux_free(&ux);
}

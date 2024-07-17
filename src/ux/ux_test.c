#include "core/core.h"
#include "utest.h"

#include "ux.h"

UTEST(CircuitUX, init) {
  CircuitUX ux;
  ErrStack errs = {0};
  ux_init(&ux, &errs, circuit_symbol_descs(), NULL, NULL);

  ux_free(&ux);
}

UTEST(CircuitUX, adding_components) {
  CircuitUX ux;
  ErrStack errs = {0};
  ux_init(&ux, &errs, circuit_symbol_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit, "AND");
  ID orSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit, "OR");

  ux_start_adding_symbol(&ux, andSymbolKindID);

  ASSERT_TRUE(circ_has(&ux.view.circuit, ux.addingSymbol));
  ASSERT_EQ(
    circ_get(&ux.view.circuit, ux.addingSymbol, SymbolKindID), andSymbolKindID);

  ID oldID = ux.addingSymbol;

  ux_change_adding_symbol(&ux, orSymbolKindID);

  ASSERT_TRUE(!circ_has(&ux.view.circuit, oldID) || oldID == ux.addingSymbol);
  ASSERT_TRUE(circ_has(&ux.view.circuit, ux.addingSymbol));
  ASSERT_EQ(
    circ_get(&ux.view.circuit, ux.addingSymbol, SymbolKindID), orSymbolKindID);

  oldID = ux.addingSymbol;

  ux_stop_adding_symbol(&ux);

  ASSERT_FALSE(circ_has(&ux.view.circuit, oldID));
  ASSERT_EQ(ux.addingSymbol, NO_ID);

  ux_free(&ux);
}

UTEST(CircuitUX, selection_and_deselection) {
  CircuitUX ux;
  ErrStack errs = {0};
  ux_init(&ux, &errs, circuit_symbol_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit, ux.view.circuit.top, andSymbolKindID);
  HMM_Vec2 position = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit, symbolID, position);

  // Test selection
  ux_select_item(&ux, symbolID);
  ASSERT_EQ(arrlen(ux.view.selected), 1);
  ASSERT_EQ(ux.view.selected[0], symbolID);

  // Test deselection
  ux_deselect_item(&ux, symbolID);
  ASSERT_EQ(arrlen(ux.view.selected), 0);

  ux_free(&ux);
}

UTEST(CircuitUX, move_component) {
  CircuitUX ux;
  ErrStack errs = {0};
  ux_init(&ux, &errs, circuit_symbol_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit, ux.view.circuit.top, andSymbolKindID);
  HMM_Vec2 initialPosition = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit, symbolID, initialPosition);

  // Select the symbol
  ux_select_item(&ux, symbolID);

  // Move the symbol
  HMM_Vec2 newPosition = HMM_V2(200, 200);
  ux_move_selection(&ux, initialPosition, newPosition, false);

  Position finalPosition = circ_get(&ux.view.circuit, symbolID, Position);
  ASSERT_EQ(finalPosition.X, newPosition.X);
  ASSERT_EQ(finalPosition.Y, newPosition.Y);

  ux_free(&ux);
}

UTEST(CircuitUX, delete_component) {
  CircuitUX ux;
  ErrStack errs = {0};
  ux_init(&ux, &errs, circuit_symbol_descs(), NULL, NULL);

  ID andSymbolKindID = circ_get_symbol_kind_by_name(&ux.view.circuit, "AND");

  ID symbolID =
    circ_add_symbol(&ux.view.circuit, ux.view.circuit.top, andSymbolKindID);
  HMM_Vec2 position = HMM_V2(100, 100);
  circ_set_symbol_position(&ux.view.circuit, symbolID, position);

  // Select the symbol
  ux_select_item(&ux, symbolID);

  // Delete the symbol
  ux_delete_selected(&ux);

  ASSERT_FALSE(circ_has(&ux.view.circuit, symbolID));

  ux_free(&ux);
}

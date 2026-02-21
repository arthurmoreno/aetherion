from aetherion import ActivateProgramCommand, Command, DropToWorldCommand, EditorAction, EditorCommand


def test_command_basic():
    cmd = Command("test_type")
    assert cmd.type == "test_type"
    assert cmd.get_type() == "test_type"

    cmd.set_type("new_type")
    assert cmd.type == "new_type"
    assert cmd.get("nonexistent", "default") == "default"


def test_activate_program_command():
    cmd = ActivateProgramCommand("prog_123")
    assert cmd.get_program_id() == "prog_123"


def test_drop_to_world_command():
    cmd = DropToWorldCommand(1, "inv", 10.0, 20.0)
    assert cmd.get_item_index() == 1
    assert cmd.get_src_window() == "inv"
    assert cmd.get_world_x() == 10.0
    assert cmd.get_world_y() == 20.0


def test_editor_command():
    cmd = EditorCommand(EditorAction.Play)
    assert cmd.get_type() == "editor_play"

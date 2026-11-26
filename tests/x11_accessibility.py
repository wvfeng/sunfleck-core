"""Exercise the X11 bridge through the AT-SPI registry in a live desktop session."""
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import gi

gi.require_version("Atspi", "2.0")
from gi.repository import Atspi


PAGE = """<Window xmlns="urn:sunfleck:v1" Title="Sunfleck M9 Accessibility" Width="500" Height="340">
  <StackPanel Padding="12">
    <TextBlock Text="Sunfleck M9"/>
    <Button Content="Apply"/>
    <CheckBox Text="Enabled"/>
    <TextBox Id="editor" Text="Accessible editor text"/>
    <PasswordBox Id="secret" Text="hidden fixture value"/>
  </StackPanel>
</Window>"""


def children(node):
    return [node.get_child_at_index(i) for i in range(node.get_child_count())]


def wait_for(probe, description):
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        result = probe()
        if result:
            return result
        time.sleep(0.1)
    raise RuntimeError(description)


def verify():
    desktop = Atspi.get_desktop(0)

    def find_window():
        return next((window for app in children(desktop) if app.get_name() == "Sunfleck"
                     for window in children(app) if window.get_name() == "Sunfleck M9 Accessibility"), None)

    window = wait_for(find_window, "Sunfleck window is missing from the AT-SPI registry")
    controls = {node.get_name(): node for node in children(window)}
    for name, node in controls.items():
        print(name, node.get_role_name(), node.get_interfaces())
    assert controls["Apply"].get_role() == Atspi.Role.PUSH_BUTTON
    editor = controls["editor"]
    password = controls["secret"]
    assert editor.get_role() == Atspi.Role.ENTRY
    assert password.get_role() == Atspi.Role.PASSWORD_TEXT
    assert Atspi.Text.get_text(editor, 0, -1) == "Accessible editor text"
    assert Atspi.Text.get_string_at_offset(editor, 5, Atspi.TextGranularity.LINE).content == "Accessible editor text"
    assert Atspi.Text.get_text(password, 0, -1) == ""
    assert Atspi.Component.grab_focus(editor)
    wait_for(lambda: window.get_state_set().contains(Atspi.StateType.ACTIVE), "Focused window is not active")
    wait_for(lambda: editor.get_state_set().contains(Atspi.StateType.FOCUSED), "Editor did not receive focus")
    toggle = controls["Enabled"]
    assert toggle.get_role() == Atspi.Role.CHECK_BOX
    assert toggle.get_state_set().contains(Atspi.StateType.CHECKABLE)
    assert Atspi.Action.do_action(toggle, 0)
    wait_for(lambda: toggle.get_state_set().contains(Atspi.StateType.CHECKED), "Checkbox did not toggle")
    return find_window


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: x11_accessibility.py /path/to/fleck_preview")
    Atspi.init()
    with tempfile.TemporaryDirectory() as directory:
        fixture = Path(directory, "accessibility.fleck")
        fixture.write_text(PAGE, encoding="utf-8")
        process = subprocess.Popen([sys.argv[1], "--renderer=software", str(fixture)])
        try:
            find_window = verify()
        finally:
            process.terminate()
            process.wait(timeout=5)
        wait_for(lambda: find_window() is None, "Closed window remains in the AT-SPI registry")
        print("AT-SPI discovery, window activation, roles, text, password protection, focus, toggle and cleanup passed.")

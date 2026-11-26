"""Keep every public C ABI entry visible; unlisted entries default to unverified on Web."""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERIFIED = set('''ui_app_create ui_app_destroy ui_app_open_window ui_app_set_window_options
ui_app_load_fleck ui_app_load_fleck_with_context ui_app_load_fleck_with_model ui_app_find_fleck_node
ui_app_poll ui_app_request_close ui_app_post ui_app_pixels
ui_text_source_create ui_text_source_set ui_text_source_destroy
ui_node_set_action ui_node_set_action_with_concurrency ui_node_set_text ui_node_get_text
ui_node_get_debug_id ui_node_bind_text ui_node_bind_command
ui_command_create ui_command_create_external ui_command_cancel ui_command_is_running
ui_completion_complete ui_completion_release
ui_model_create ui_model_add_property ui_model_add_text_source ui_model_add_command
ui_model_notify_property_changed ui_model_destroy ui_node_bind_property
ui_dock_create ui_dock_find ui_dock_float ui_dock_reset ui_dock_get_windows ui_dock_set_event
ui_app_register_dock_skin
ui_document_load ui_document_scroll_to ui_document_set_scroll ui_document_get_scroll
ui_document_selection ui_document_select_all ui_document_take_event
ui_document_scroll_horizontal ui_document_set_font'''.split())
UNSUPPORTED = {'ui_app_run', 'ui_app_create_window', 'ui_app_set_debug_picking', 'ui_app_enable_debug_server'}

def inventory():
    header = (ROOT / 'include/sunfleck/ui.h').read_text(encoding='utf-8')
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', header, flags=re.S)
    names = sorted(set(re.findall(r'^\s*(?:ui_result|void|uint32_t|const (?:uint8_t|char)\*)\s+(ui_\w+)\s*\(', clean, re.M)))
    if not (VERIFIED | UNSUPPORTED) <= set(names):
        raise ValueError('The support classification references a removed C API')
    native = [(p, p.read_text(encoding='utf-8')) for p in (ROOT / 'bindings/csharp').glob('*.cs')]
    cpp = [(p, p.read_text(encoding='utf-8')) for p in (ROOT / 'include/sunfleck').glob('*.hpp')]
    records = []
    for name in names:
        status = 'hosted-covered' if name.startswith('ui_host_') or name in VERIFIED else 'not-in-web-acceptance'
        if name in UNSUPPORTED:
            status = 'unsupported-operation'
        if name == 'ui_command_create_async':
            status = 'worker-execution-unsupported'
        if name == 'ui_app_poll':
            status = 'poll-zero-only'
        records.append({'c': name, 'status': status,
                        'cppWrappers': [p.relative_to(ROOT).as_posix() for p, text in cpp if re.search(r'\b' + name + r'\s*\(', text)],
                        'csharpInterop': [p.relative_to(ROOT).as_posix() for p, text in native if f'EntryPoint = "{name}"' in text]})
    return {'version': 1, 'meaning': 'Covered means used by the hosted path or representative acceptance, not exhaustive parameters/control parity. Missing interop is not proof of unsupported C ABI.', 'apis': records}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    output = ROOT / 'docs/web-api-inventory.json'
    data = json.dumps(inventory(), indent=2, ensure_ascii=False) + '\n'
    if args.check:
        if not output.exists() or output.read_text(encoding='utf-8') != data:
            raise SystemExit('API inventory changed: run python tools/web/api-inventory.py')
    else:
        output.write_text(data, encoding='utf-8')
    print(f'{len(inventory()["apis"])} public C ABI entries indexed')

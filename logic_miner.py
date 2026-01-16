import os
import ast
import hashlib
import json
import re
import pandas as pd
from typing import Dict, List, Tuple, Any, Optional

# --- CONFIGURATION ---
TARGET_DIR = "/Users/yamaguchinaoyuki/Desktop/JJJ/AAAAA"
OUTPUT_FILE = "/Users/yamaguchinaoyuki/Desktop/JJJ/AAAAA/A.xlsx"

EXCLUDED_EXTENSIONS = {
    '.safetensors', '.bin', '.pth', '.pt', '.onnx', '.gguf', 
    '.xlsx', '.xls', '.csv', '.db', '.sqlite',               
    '.png', '.jpg', '.jpeg', '.gif', '.ico', '.svg',         
    '.ttf', '.otf', '.woff', '.woff2',                       
    '.DS_Store', '.pkl', '.pyc',                             
    '.zip', '.tar', '.gz'
}

EXCLUDED_DIRS = {
    '.venv', '__pycache__', '.git', 'build', 'dist', 
    'node_modules', '.idea', '.vscode'
}

# --- LOGIC VAULT (ID & Data Management) ---
class LogicVault:
    def __init__(self):
        self.logic_library: Dict[str, Dict] = {}  
        self.unique_components: Dict[str, Dict] = {}  
        self.blueprints: List[Dict] = []
        self.recipe_counter = 0
        self.component_counter = 0

    def get_next_recipe_id(self):
        self.recipe_counter += 1
        return f"R-{self.recipe_counter:03d}"

    def get_next_component_id(self):
        self.component_counter += 1
        return f"U-{self.component_counter:03d}"

    def register_component(self, value: str, c_type: str, origin: str) -> str:
        val = value.strip()
        if not val and c_type != 'string_literal':
            return "U-NULL"
        
        global_types = ('string_literal', 'number_literal', 'module', 'header_include', 'attribute')
        key = f"{c_type}:{val}" if c_type in global_types else f"{c_type}:{val}:{origin}"

        if key in self.unique_components:
            return self.unique_components[key]['unique_id']
        
        uid = self.get_next_component_id()
        self.unique_components[key] = {
            'unique_id': uid, 'type': c_type, 'value': val,
            'origin': origin if c_type not in global_types else "GLOBAL"
        }
        return uid

    def register_logic(self, template: str, name: str, description: str) -> str:
        norm_template = '\n'.join([line.strip() for line in template.splitlines() if line.strip()])
        template_hash = hashlib.md5(norm_template.encode('utf-8')).hexdigest()
        
        if template_hash in self.logic_library:
            return self.logic_library[template_hash]['recipe_id']
        
        rid = self.get_next_recipe_id()
        self.logic_library[template_hash] = {
            'recipe_id': rid, 'name': name, 'template': template, 'description': description
        }
        return rid

    def add_blueprint(self, file_id, sequence, recipe_id, slot_mapping, indent_level):
        clean_mapping = {k: v for k, v in slot_mapping.items() if v != "U-NULL"}
        self.blueprints.append({
            'file_id': file_id, 'sequence': sequence, 'recipe_id': recipe_id,
            'slot_mapping': json.dumps(clean_mapping, ensure_ascii=False),
            'indent_level': indent_level
        })

# --- ANALYZERS ---
class PythonAtomizer(ast.NodeTransformer):
    def __init__(self, vault: LogicVault, file_path: str):
        self.vault, self.file_path, self.slot_mapping, self.slot_counter = vault, file_path, {}, 0
        
    def _allocate_slot(self, value: str, c_type: str) -> str:
        uid = self.vault.register_component(value, c_type, self.file_path)
        if uid == "U-NULL": return ""
        for slot, existing_uid in self.slot_mapping.items():
            if existing_uid == uid: return f"{{{slot}}}"
        self.slot_counter += 1
        self.slot_mapping[str(self.slot_counter)] = uid
        return f"{{{self.slot_counter}}}"

    def visit_Name(self, node):
        placeholder = self._allocate_slot(node.id, 'variable')
        if placeholder: node.id = placeholder.replace("{", "").replace("}", "")
        return node

    def visit_Constant(self, node):
        if isinstance(node.value, (str, int, float)):
            c_type = 'string_literal' if isinstance(node.value, str) else 'number_literal'
            placeholder = self._allocate_slot(str(node.value), c_type)
            if placeholder: node.value = placeholder
        return node
    
    def visit_Attribute(self, node):
        self.generic_visit(node)
        placeholder = self._allocate_slot(node.attr, 'attribute')
        if placeholder: node.attr = placeholder.replace("{", "").replace("}", "")
        return node

class TextAtomizer:
    def __init__(self, vault: LogicVault, file_path: str):
        self.vault, self.file_path, self.slot_mapping, self.slot_counter = vault, file_path, {}, 0

    def _allocate_slot(self, value: str, c_type: str) -> str:
        uid = self.vault.register_component(value, c_type, self.file_path)
        if uid == "U-NULL": return value
        for slot, existing_uid in self.slot_mapping.items():
            if existing_uid == uid: return f"{{{slot}}}"
        self.slot_counter += 1
        self.slot_mapping[str(self.slot_counter)] = uid
        return f"{{{self.slot_counter}}}"

    def atomize(self, content: str) -> str:
        content = re.sub(r'#include\s*(?:<([^>]+)>|"([^"]+)")', 
                         lambda m: f'#include {self._allocate_slot(m.group(1) or m.group(2), "header_include")}', content)
        content = re.sub(r'"([^"\\]*(?:\\.[^"\\]*)*)"', 
                         lambda m: f'"{self._allocate_slot(m.group(1), "string_literal")}"', content)
        return content

# --- UNIFIED VALIDATOR ---
def run_health_check(vault: LogicVault):
    print("\n--- 🩺 Health Check Start ---")
    errors = []
    valid_uids = {c['unique_id'] for c in vault.unique_components.values()}
    valid_rids = {r['recipe_id'] for r in vault.logic_library.values()}

    for bp in vault.blueprints:
        if bp['recipe_id'] not in valid_rids:
            errors.append(f"Missing Recipe: {bp['recipe_id']} in file {bp['file_id']}")
        try:
            mapping = json.loads(bp['slot_mapping'])
            for uid in mapping.values():
                if uid not in valid_uids:
                    errors.append(f"Missing Component: {uid} referenced in {bp['file_id']}")
        except:
            errors.append(f"Invalid JSON mapping in {bp['file_id']}")

    # 言語固有チェック
    for r in vault.logic_library.values():
        if "#include" in r['template'] and "{" not in r['template']:
            errors.append(f"C++ Check: Logic {r['recipe_id']} has include but no placeholders.")

    for c in vault.unique_components.values():
        if c['type'] == 'syntax_error':
            errors.append(f"Critical: Syntax error in file {c['value']}")

    if not errors:
        print("✅ PASSED: Logic structure is perfectly consistent.")
    else:
        print(f"❌ FAILED: Found {len(errors)} issues.")
        for e in errors[:5]: print(f"  - {e}")
    print("--- 🩺 Health Check End ---\n")

# --- MAIN PROCESS ---
def main():
    print("🚀 Logic Miner & Health Checker (Unified) Started...")
    vault = LogicVault()
    
    for root, dirs, files in os.walk(TARGET_DIR):
        dirs[:] = [d for d in dirs if d not in EXCLUDED_DIRS]
        for file in files:
            file_path = os.path.join(root, file)
            _, ext = os.path.splitext(file)
            # --- 修正：自分と自分の生成物を徹底排除 ---
            if (file == 'logic_miner.py' or 
                file == 'logic_B_miner.py' or 
                file.startswith('A.xlsx') or 
                file.startswith('~$') or
                ext == '.xlsx' or 
                ext == '.csv'):
                continue
            
            # models が含まれる場合は拡張子を問わずスキップ
            if 'models' in file_path:
                continue

            if ext in EXCLUDED_EXTENSIONS: continue
            try:
                if os.path.getsize(file_path) > 1024 * 1024: continue
                with open(file_path, 'r', encoding='utf-8') as f: content = f.read()
                if ext == '.py':
                    rel_path = os.path.relpath(file_path, TARGET_DIR)
                    try:
                        tree = ast.parse(content)
                        for seq, node in enumerate(tree.body, 1):
                            atomizer = PythonAtomizer(vault, rel_path)
                            template = ast.unparse(atomizer.visit(node))
                            rid = vault.register_logic(template, f"Op_{type(node).__name__}", "Python Logic")
                            vault.add_blueprint(rel_path, seq, rid, atomizer.slot_mapping, getattr(node, 'col_offset', 0))
                    except SyntaxError:
                        vault.register_component(rel_path, 'syntax_error', 'parser')
                else:
                    rel_path = os.path.relpath(file_path, TARGET_DIR)
                    atomizer = TextAtomizer(vault, rel_path)
                    rid = vault.register_logic(atomizer.atomize(content), f"File_{file}", "Text Content")
                    vault.add_blueprint(rel_path, 1, rid, atomizer.slot_mapping, 0)
            except: continue

    run_health_check(vault)

    print(f"💾 Saving to {OUTPUT_FILE}...")
    try:
        with pd.ExcelWriter(OUTPUT_FILE, engine='openpyxl') as writer:
            pd.DataFrame(vault.logic_library.values()).to_excel(writer, sheet_name='Logic_Library', index=False)
            pd.DataFrame(vault.unique_components.values()).to_excel(writer, sheet_name='Unique_Components', index=False)
            pd.DataFrame(vault.blueprints).to_excel(writer, sheet_name='Project_Blueprints', index=False)
        print("🎉 All tasks completed successfully.")
    except Exception as e:
        print(f"❌ Save Failed: {e}")

if __name__ == "__main__":
    main()
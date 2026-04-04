from __future__ import annotations
import lldb
import os
import json

def _read_atom_str(process, atom_ptr):
    """Read the string content of an Atom_ at the given address."""
    if atom_ptr == 0:
        return None
    err = lldb.SBError()
    header = process.ReadMemory(atom_ptr, 4, err)
    if err.Fail():
        return None
    length = int.from_bytes(header, 'little') & 0x7fffffff
    if length == 0:
        return ''
    read_len = min(length, 256)
    data = process.ReadMemory(atom_ptr + 8, read_len, err)
    if err.Fail():
        return None
    s = data.decode('utf-8', errors='replace')
    if length > 256:
        return s + '...'
    return s

def atom_summary(valobj, internal_dict):
    ptr = valobj.GetValueAsUnsigned(0)
    if ptr == 0:
        return 'nil'
    name = _read_atom_str(valobj.process, ptr)
    if name is None:
        return f'<err @ {hex(ptr)}>'
    if name == '':
        return 'Atom("")'
    return f'Atom("{name}")'

def string_view_summary(valobj, internal_dict):
    tn = valobj.GetTypeName()
    if '*' in tn:
        return hex(valobj.GetValueAsUnsigned())
    if 'Long' in tn:
        pre = 'LS'
    else:
        pre = 'SV'
    length = valobj.GetChildMemberWithName('length').GetValueAsUnsigned(0)
    if length == 0: return f'{pre}("")'
    truncated = False
    if length > 1024:
        length = 1024
        truncated = True
    ptr = valobj.GetChildMemberWithName('text').GetValueAsUnsigned(0)
    if ptr == 0:
        return '(nil)'
    err = lldb.SBError()
    buff = valobj.process.ReadMemory(ptr, length, err)
    if err.Fail():
        return f'<error: {err}>'
    result = repr(buff.decode(encoding='utf-8', errors='ignore'))
    if '\\x00' in result:
        return f'{pre}(garbage)'
    if result[0] == "'":
        result = '"' + result[1:-1].replace("\\'", "'").replace('"', '\\"') + '"'
    if truncated:
        return f'{pre}({result}..truncated)'
    return f'{pre}({result})'

# --- CcQualType ---
#
# CcQualType is a bit-packed union:
#   bits[0]   = is_const
#   bits[1]   = is_volatile
#   bits[2]   = is_atomic
#   bits[3..] = ptr (if < CCBT_COUNT, it's a CcBasicTypeKind;
#                     otherwise mask off low 3 bits to get a type pointer)

_BASIC_NAMES = [
    '<invalid>', 'void', '_Bool', 'char', 'signed char', 'unsigned char',
    'short', 'unsigned short', 'int', 'unsigned int', 'long', 'unsigned long',
    'long long', 'unsigned long long', '__int128', 'unsigned __int128',
    '_Float16', 'float', 'double', 'long double', '_Float128',
    'float _Complex', 'double _Complex', 'long double _Complex',
    'nullptr_t', '_Type',
]
_CCBT_COUNT = len(_BASIC_NAMES)

_lldb_type_cache = {}

def _make_typed_val(target, addr, type_name):
    """Create an SBValue by interpreting memory at addr as type_name."""
    if type_name not in _lldb_type_cache:
        _lldb_type_cache[type_name] = target.FindFirstType(type_name)
    lldb_type = _lldb_type_cache[type_name]
    if not lldb_type.IsValid():
        return None
    val = target.CreateValueFromAddress('_qt', lldb.SBAddress(addr, target), lldb_type)
    if not val.IsValid():
        return None
    return val

def _child_bits(val, field):
    """Get the .bits member of a CcQualType field on val."""
    child = val.GetChildMemberWithName(field)
    if not child.IsValid():
        return 0
    b = child.GetChildMemberWithName('bits')
    if not b.IsValid():
        return 0
    return b.GetValueAsUnsigned(0)

def _describe_qualtype(process, bits, depth=0):
    if bits == 0:
        return '<invalid>'
    if depth > 8:
        return '...'
    quals = bits & 7
    ptr_val = bits >> 3
    qp = ''
    if quals & 1: qp += 'const '
    if quals & 2: qp += 'volatile '
    if quals & 4: qp += '_Atomic '
    # Basic type: upper bits are a small enum value
    if ptr_val < _CCBT_COUNT:
        return f'{qp}{_BASIC_NAMES[ptr_val]}'
    type_ptr = bits & ~7
    err = lldb.SBError()
    kind_data = process.ReadMemory(type_ptr, 4, err)
    if err.Fail():
        return f'{qp}<err>'
    flags = int.from_bytes(kind_data, 'little')
    kind = flags & 0xf
    target = process.GetTarget()
    # struct(3), union(4), enum(1)
    if kind in (1, 3, 4):
        prefix = {1: 'enum', 3: 'struct', 4: 'union'}[kind]
        type_name = {1: 'CcEnum', 3: 'CcStruct', 4: 'CcUnion'}[kind]
        val = _make_typed_val(target, type_ptr, type_name)
        if val is None:
            return f'{qp}{prefix} <?>'
        atom_ptr = val.GetChildMemberWithName('name').GetValueAsUnsigned(0)
        name = _read_atom_str(process, atom_ptr)
        return f'{qp}{prefix} {name}' if name else f'{qp}{prefix} <anon>'
    # pointer(2)
    if kind == 2:
        is_restrict = (flags >> 4) & 1
        val = _make_typed_val(target, type_ptr, 'CcPointer')
        if val is None:
            return f'{qp}<ptr ?>'
        pointee_bits = _child_bits(val, 'pointee')
        inner = _describe_qualtype(process, pointee_bits, depth + 1)
        ptr_quals = qp.rstrip()
        if is_restrict:
            ptr_quals = f'{ptr_quals} restrict'.lstrip()
        if ptr_quals:
            return f'{inner} *{ptr_quals}'
        return f'{inner} *'
    # array(6)
    if kind == 6:
        is_incomplete = (flags >> 6) & 1
        val = _make_typed_val(target, type_ptr, 'CcArray')
        if val is None:
            return f'{qp}<array ?>'
        elem_bits = _child_bits(val, 'element')
        inner = _describe_qualtype(process, elem_bits, depth + 1)
        if is_incomplete:
            return f'{qp}{inner}[]'
        length = val.GetChildMemberWithName('length').GetValueAsUnsigned(0)
        return f'{qp}{inner}[{length}]'
    # function(5)
    if kind == 5:
        is_variadic = (flags >> 4) & 1
        no_prototype = (flags >> 5) & 1
        val = _make_typed_val(target, type_ptr, 'CcFunction')
        if val is None:
            return f'{qp}<func ?>'
        ret_bits = _child_bits(val, 'return_type')
        ret_str = _describe_qualtype(process, ret_bits, depth + 1)
        param_count = val.GetChildMemberWithName('param_count').GetValueAsUnsigned(0)
        params = []
        for i in range(min(param_count, 8)):
            p = val.GetValueForExpressionPath(f'.params[{i}]')
            if not p or not p.IsValid():
                params.append('?')
                break
            p_bits = p.GetChildMemberWithName('bits').GetValueAsUnsigned(0)
            params.append(_describe_qualtype(process, p_bits, depth + 1))
        if param_count > 8:
            params.append('...')
        elif is_variadic:
            params.append('...')
        if not params:
            if no_prototype:
                return f'{qp}{ret_str}()'
            return f'{qp}{ret_str}(void)'
        return f'{qp}{ret_str}({", ".join(params)})'
    return f'{qp}<kind={kind}>'

def ccqualtype_summary(valobj, internal_dict):
    bits = valobj.GetChildMemberWithName('bits').GetValueAsUnsigned(0)
    return _describe_qualtype(valobj.process, bits)

def dyn_array(lengthname, dataname, correction):
    class Synth:
        def __init__(self, valobj, internal_dict):
            self.obj = valobj
        def num_children(self):
            length = self.obj.GetChildMemberWithName(lengthname).GetValueAsUnsigned(0)+correction
            return length
        def has_children(self):
            return self.num_children() != 0
        def get_child_at_index(self, index):
            return self.obj.GetValueForExpressionPath(f'.{dataname}[{index}]')
        def get_child_index(self,name):
            try:
                return int(name.lstrip('[').rstrip(']'))
            except:
                return None
    return Synth

MaSynth = dyn_array('count', 'data', 0)

def stack_summary(valobj, internal_dict):
    lengthname = 'current'
    correction = 1
    length = valobj.GetChildMemberWithName(lengthname).GetValueAsUnsigned(0)+correction
    valname = 'stack'
    objs = valobj.GetChildMemberWithName(valname)
    children = []
    for i in range(length):
        children.append(objs.GetChildAtIndex(i))
    return '[\n  ' + ',\n  '.join(str(o) for o in children) + '\n]'

IN_VIM = os.environ.get('VIM_TERMINAL')

def recenter(debugger, command, result, internal_dict):
    for thread in debugger.GetTargetAtIndex(0).process:
        if (thread.GetStopReason() != lldb.eStopReasonNone) and (thread.GetStopReason() != lldb.eStopReasonInvalid):
            frame = thread.GetSelectedFrame()
            return _recenter(frame)

def _recenter(frame:lldb.SBFrame) -> bool:
    if not IN_VIM: return
    le = frame.line_entry
    f = le.file
    l = le.line
    if not l: return
    if not f.fullpath: return
    path = os.path.relpath(f.fullpath).replace(' ', '\\ ')
    js = json.dumps(['call', 'Tapi_open', [path, l]])
    print('\033]51;', js, '\07', end='', sep='', flush=True)
    return

class StopHook:
    def __init__(self, target:lldb.SBTarget, extra_args:lldb.SBStructuredData, internal_dict:dict) -> None:
        pass #?
    def handle_stop(self, exe_ctx: lldb.SBExecutionContext, stream: lldb.SBStream) -> bool:
        _recenter(exe_ctx.frame)
        return True

def __lldb_init_module(debugger, internal_dict):
    modname = os.path.splitext(os.path.basename(__file__))[0]
    debugger.HandleCommand(f'type summary add Atom -F {modname}.atom_summary')
    debugger.HandleCommand(f'type summary add StringView -F {modname}.string_view_summary')
    debugger.HandleCommand(f'type summary add LongString -F {modname}.string_view_summary')
    debugger.HandleCommand(f'type summary add CcQualType -F {modname}.ccqualtype_summary')
    debugger.HandleCommand(f'type synthetic add -x "^ma__" --python-class {modname}.MaSynth')
    debugger.HandleCommand(f'type summary add -x "^ma__" --expand --summary-string "${{var.count}}/${{var.capacity}} items"')
    debugger.HandleCommand(f'command script add -f {modname}.recenter rc')

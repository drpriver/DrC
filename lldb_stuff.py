from __future__ import annotations
import lldb
import os
import json

def string_view_summary(valobj, internal_dict):
    tn = valobj.GetTypeName()
    if '*' in tn:
        return hex(valobj.GetValueAsUnsigned())
    length = valobj.GetChildMemberWithName('length').GetValueAsUnsigned(0)
    if length == 0: return '""'
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
    if 'Long' in tn:
        pre = 'LS'
    else:
        pre = 'SV'
    if '\\x00' in result:
        return f'{pre}(garbage)'
    if result[0] == "'":
        result = '"' + result[1:-1].replace("\\'", "'").replace('"', '\\"') + '"'
    if truncated:
        return f'{pre}({result}..truncated)'
    return f'{pre}({result})'

def stack_summary(valobj, internal_dict):
    print(type(internal_dict))
    print(internal_dict.keys())
    lengthname = 'current'
    correction = 1
    length = valobj.GetChildMemberWithName(lengthname).GetValueAsUnsigned(0)+correction
    valname = 'stack'
    objs = valobj.GetChildMemberWithName(valname)
    children = []
    for i in range(length):
        children.append(objs.GetChildAtIndex(i))
    return '[\n  ' + ',\n  '.join(str(o) for o in children) + '\n]'

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
                return int(name.lstrip('[').rstrip('['))
            except:
                return None
    return Synth

def __lldb_init_module(debugger, internal_dict):
    modname = os.path.splitext(os.path.basename(__file__))[0]
    # debugger.HandleCommand(f'type summary add StringView -F {modname}.string_view_summary')
    # debugger.HandleCommand(f'type summary add LongString -F {modname}.string_view_summary')
    def reg(cls, lengthname, dataname, correction=0):
        globals()[cls] = dyn_array(lengthname, dataname, correction)
        debugger.HandleCommand(f'type synthetic add {cls} --python-class {modname}.{cls}')
        debugger.HandleCommand(f'type summary add --expand {cls} --summary-string "${{svar%#}} items"')
    def mreg(t):
        reg('Marray__'+t, 'count', 'data')
    # mreg('StringView')
    # mreg('LoadedSource')
    # mreg('LongString')
    # mreg('NodeHandle')
    # mreg('IdItem')
    # mreg('Node')
    debugger.HandleCommand(f'command script add -f {modname}.recenter rc')

def recenter(debugger, command, result, internal_dict):
    for thread in debugger.GetTargetAtIndex(0).process:
        if (thread.GetStopReason() != lldb.eStopReasonNone) and (thread.GetStopReason() != lldb.eStopReasonInvalid):
            frame = thread.GetSelectedFrame()
            return _recenter(frame)

IN_VIM = os.environ.get('VIM_TERMINAL')

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

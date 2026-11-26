#define NOMINMAX
#include "windows_accessibility.h"
#include <wrl.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <mutex>
using Microsoft::WRL::ComPtr;
namespace accessibility {
class Bridge {
public:
    HWND hwnd;
    std::mutex mutex;
    std::map<UINT_PTR, std::shared_ptr<Request>> requests;
    explicit Bridge(HWND h) : hwnd(h) {}
};
Window create(HWND window) { return std::make_shared<Bridge>(window); }

LRESULT dispatch(const Window& window, WPARAM ticket, void* app, Handler handler)
{
    if (!window) return UIA_E_ELEMENTNOTAVAILABLE;
    std::shared_ptr<Request> request;
    {
        std::lock_guard lock(window->mutex);
        auto pending = window->requests.extract(ticket);
        if (pending.empty()) return UIA_E_ELEMENTNOTAVAILABLE;
        request = std::move(pending.mapped());
    }
    try { return handler(app, *request); }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

class Provider final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IRawElementProviderSimple, IRawElementProviderFragmentRoot, IRawElementProviderFragment,
    IInvokeProvider, IToggleProvider, IRangeValueProvider, IValueProvider, IExpandCollapseProvider, ISelectionItemProvider, ISelectionProvider> {
    std::weak_ptr<Bridge> bridge;
    uint64_t id;

    HRESULT send(Request& r) const {
        auto window = bridge.lock();
        if (!window) return UIA_E_ELEMENTNOTAVAILABLE;
        static std::atomic<UINT_PTR> next{1};
        const auto ticket = next.fetch_add(1);
        auto request = std::make_shared<Request>(std::move(r));
        {
            std::lock_guard lock(window->mutex);
            window->requests.emplace(ticket,request);
        }
        DWORD_PTR result{};
        const auto ok = SendMessageTimeoutW(window->hwnd,request_message,ticket,0,
            SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT,3000,&result);
        std::lock_guard lock(window->mutex);
        if (window->requests.erase(ticket) || !ok) return UIA_E_ELEMENTNOTAVAILABLE;
        r = std::move(*request);
        return static_cast<HRESULT>(result);
    }
    template<class F> HRESULT read(F f) const noexcept {
        try {
            Request r;
            const auto status = send(r);
            if (FAILED(status)) return status;
            for (const auto& e : r.elements) if (e.id == id) return f(e,r.elements);
            return UIA_E_ELEMENTNOTAVAILABLE;
        } catch (...) { return E_OUTOFMEMORY; }
    }
    HRESULT action(Action a, unsigned number = 0, LPCWSTR text = nullptr) noexcept {
        try {
            Request r; r.action=a; r.target=id; r.number=number;
            if (text) r.text=text;
            return send(r);
        } catch (...) { return E_OUTOFMEMORY; }
    }
    template<class I> HRESULT element(uint64_t target,I** out) const {
        auto p = Microsoft::WRL::Make<Provider>(bridge,target);
        return p ? p.CopyTo(out) : E_OUTOFMEMORY;
    }
    static int type(ui_node_kind k) {
        switch(k) {
        case UI_BUTTON: case UI_ICON_BUTTON: case UI_TOGGLE_BUTTON: return UIA_ButtonControlTypeId;
        case UI_CHECK_BOX: case UI_SWITCH: return UIA_CheckBoxControlTypeId;
        case UI_RADIO_BUTTON: return UIA_RadioButtonControlTypeId;
        case UI_SLIDER: return UIA_SliderControlTypeId;
        case UI_SPIN_BOX: return UIA_SpinnerControlTypeId;
        case UI_PROGRESS_BAR: return UIA_ProgressBarControlTypeId;
        case UI_COMBO_BOX: return UIA_ComboBoxControlTypeId;
        case UI_TEXT_BOX: case UI_PASSWORD_BOX: case UI_TEXT_AREA: case UI_SEARCH_BOX: return UIA_EditControlTypeId;
        case UI_LIST_VIEW: return UIA_ListControlTypeId;
        case UI_LIST_ITEM: return UIA_ListItemControlTypeId;
        case UI_TAB_VIEW: return UIA_TabControlTypeId;
        case UI_TAB_ITEM: return UIA_TabItemControlTypeId;
        case UI_TREE_VIEW: return UIA_TreeControlTypeId;
        case UI_TREE_ITEM: return UIA_TreeItemControlTypeId;
        case UI_HYPERLINK: return UIA_HyperlinkControlTypeId;
        case UI_DOCUMENT_VIEW: return UIA_DocumentControlTypeId;
        case UI_TEXT_BLOCK: case UI_LABEL: return UIA_TextControlTypeId;
        default: return UIA_GroupControlTypeId;
        }
    }
public:
    Provider(std::weak_ptr<Bridge> b,uint64_t target) : bridge(std::move(b)), id(target) {}
    IFACEMETHODIMP get_ProviderOptions(ProviderOptions* out) override { *out=ProviderOptions_ServerSideProvider; return S_OK; }
    IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** out) override {
        *out=nullptr; if(id) return S_OK;
        auto b=bridge.lock(); return b ? UiaHostProviderFromHwnd(b->hwnd,out) : UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP GetPatternProvider(PATTERNID pattern,IUnknown** out) override {
        *out=nullptr;
        return read([&](const Element& e,const auto&) -> HRESULT {
            if (!id) return S_OK;
            const auto k=e.kind;
            if (pattern==UIA_InvokePatternId && (k==UI_BUTTON || k==UI_ICON_BUTTON || k==UI_HYPERLINK)) *out=static_cast<IInvokeProvider*>(this);
            if (pattern==UIA_TogglePatternId && (k==UI_TOGGLE_BUTTON || k==UI_CHECK_BOX || k==UI_SWITCH)) *out=static_cast<IToggleProvider*>(this);
            if (pattern==UIA_RangeValuePatternId && (k==UI_SLIDER || k==UI_PROGRESS_BAR || k==UI_SPIN_BOX)) *out=static_cast<IRangeValueProvider*>(this);
            if (pattern==UIA_ValuePatternId && type(k)==UIA_EditControlTypeId) *out=static_cast<IValueProvider*>(this);
            if (pattern==UIA_ExpandCollapsePatternId && (k==UI_TREE_ITEM || k==UI_COMBO_BOX)) *out=static_cast<IExpandCollapseProvider*>(this);
            if (pattern==UIA_SelectionItemPatternId && (k==UI_LIST_ITEM || k==UI_RADIO_BUTTON || k==UI_TAB_ITEM || k==UI_TREE_ITEM)) *out=static_cast<ISelectionItemProvider*>(this);
            if (pattern==UIA_SelectionPatternId && (k==UI_LIST_VIEW || k==UI_TREE_VIEW || k==UI_TAB_VIEW || k==UI_COMBO_BOX)) *out=static_cast<ISelectionProvider*>(this);
            if (*out) (*out)->AddRef();
            return S_OK;
        });
    }
    IFACEMETHODIMP GetPropertyValue(PROPERTYID property,VARIANT* out) override {
        VariantInit(out);
        return read([&](const Element& e,const auto&) -> HRESULT {
            if (property==UIA_ControlTypePropertyId) { out->vt=VT_I4; out->lVal=id ? type(e.kind) : UIA_WindowControlTypeId; }
            else if (property==UIA_NamePropertyId || property==UIA_AutomationIdPropertyId || property==UIA_FrameworkIdPropertyId) {
                out->vt=VT_BSTR;
                out->bstrVal=SysAllocString(property==UIA_NamePropertyId ? e.name.c_str() : property==UIA_AutomationIdPropertyId ? e.automation_id.c_str() : L"Sunfleck");
                if(!out->bstrVal) return E_OUTOFMEMORY;
            } else {
                bool value;
                switch(property) {
                case UIA_IsEnabledPropertyId: value=e.enabled; break;
                case UIA_IsKeyboardFocusablePropertyId: value=e.focusable; break;
                case UIA_HasKeyboardFocusPropertyId: value=e.focused; break;
                case UIA_IsPasswordPropertyId: value=id && e.kind==UI_PASSWORD_BOX; break;
                case UIA_IsContentElementPropertyId: case UIA_IsControlElementPropertyId: value=true; break;
                default: return S_OK;
                }
                out->vt=VT_BOOL; out->boolVal=value ? VARIANT_TRUE : VARIANT_FALSE;
            }
            return S_OK;
        });
    }
    IFACEMETHODIMP Navigate(NavigateDirection dir,IRawElementProviderFragment** out) override {
        *out=nullptr;
        return read([&](const Element& e,const auto& nodes) -> HRESULT {
            if(dir==NavigateDirection_Parent) return id ? element(e.parent,out) : S_OK;
            const Element* found=nullptr;
            for(const auto& candidate:nodes) {
                if (!candidate.id) continue;
                if(dir==NavigateDirection_FirstChild || dir==NavigateDirection_LastChild) {
                    if(candidate.parent==id) { found=&candidate; if(dir==NavigateDirection_FirstChild) break; }
                } else if(id && candidate.parent==e.parent) {
                    if(dir==NavigateDirection_PreviousSibling && candidate.id==id) break;
                    if(dir==NavigateDirection_NextSibling && found) {found=&candidate; break;}
                    if(dir==NavigateDirection_PreviousSibling || candidate.id==id) found=&candidate;
                }
            }
            return found && found->id!=id ? element(found->id,out) : S_OK;
        });
    }
    IFACEMETHODIMP GetRuntimeId(SAFEARRAY** out) override {
        *out=nullptr; if (!id) return S_OK;
        auto a=SafeArrayCreateVector(VT_I4,0,3); if(!a) return E_OUTOFMEMORY;
        LONG values[]={UiaAppendRuntimeId,static_cast<LONG>(id>>32),static_cast<LONG>(id)};
        for(LONG i=0;i<3;++i) SafeArrayPutElement(a,&i,&values[i]);
        *out=a; return S_OK;
    }
    IFACEMETHODIMP get_BoundingRectangle(UiaRect* out) override {
        return read([&](const Element& e,const auto&) {auto r=e.screen; *out={double(r.left),double(r.top),double(r.right-r.left),double(r.bottom-r.top)};return S_OK;});
    }
    IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** out) override {*out=nullptr;return S_OK;}
    IFACEMETHODIMP SetFocus() override {return action(Action::focus);}
    IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** out) override {return element(0,out);}
    IFACEMETHODIMP ElementProviderFromPoint(double x,double y,IRawElementProviderFragment** out) override {
        *out=nullptr;
        return read([&](const Element&,const auto& nodes) -> HRESULT {
            for(auto it=nodes.rbegin();it!=nodes.rend();++it) {
                auto r=it->screen; if(x>=r.left && x<r.right && y>=r.top && y<r.bottom) return element(it->id,out);
            }
            return S_OK;
        });
    }
    IFACEMETHODIMP GetFocus(IRawElementProviderFragment** out) override {
        *out=nullptr; return read([&](const Element&,const auto& nodes) -> HRESULT {
            for(const auto& e:nodes) if(e.focused) return element(e.id,out);
            return S_OK;
        });
    }
    IFACEMETHODIMP Invoke() override {return action(Action::invoke);}
    IFACEMETHODIMP Toggle() override {return action(Action::toggle);}
    IFACEMETHODIMP get_ToggleState(ToggleState* out) override {return read([&](const Element& e,const auto&){*out=e.number ? ToggleState_On : ToggleState_Off;return S_OK;});}
    IFACEMETHODIMP SetValue(double v) override {return std::isfinite(v) && v>=0 && v<=100 ? action(Action::number,static_cast<unsigned>(std::round(v))) : E_INVALIDARG;}
    IFACEMETHODIMP get_Value(double* out) override {return read([&](const Element& e,const auto&){*out=e.number;return S_OK;});}
    IFACEMETHODIMP get_IsReadOnly(BOOL* out) override {return read([&](const Element& e,const auto&){*out=e.kind==UI_PROGRESS_BAR;return S_OK;});}
    IFACEMETHODIMP get_Maximum(double* out) override {*out=100;return S_OK;}
    IFACEMETHODIMP get_Minimum(double* out) override {*out=0;return S_OK;}
    IFACEMETHODIMP get_LargeChange(double* out) override {*out=10;return S_OK;}
    IFACEMETHODIMP get_SmallChange(double* out) override {*out=1;return S_OK;}
    IFACEMETHODIMP SetValue(LPCWSTR value) override {return value ? action(Action::text,0,value) : E_INVALIDARG;}
    IFACEMETHODIMP get_Value(BSTR* out) override {
        *out=nullptr;return read([&](const Element& e,const auto&) -> HRESULT {
            if(e.kind==UI_PASSWORD_BOX) return E_ACCESSDENIED;
            *out=SysAllocString(e.value.c_str());return *out ? S_OK : E_OUTOFMEMORY;
        });
    }
    IFACEMETHODIMP Expand() override {return action(Action::expand);}
    IFACEMETHODIMP Collapse() override {return action(Action::collapse);}
    IFACEMETHODIMP get_ExpandCollapseState(ExpandCollapseState* out) override {return read([&](const Element& e,const auto&){*out=e.expanded ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;return S_OK;});}
    IFACEMETHODIMP GetSelection(SAFEARRAY** out) override {
        *out=nullptr;
        return read([&](const Element&,const auto& nodes) -> HRESULT {
            auto found=std::find_if(nodes.begin(),nodes.end(),[&](const auto& e) {
                return e.id && e.parent==id && e.selected;
            });
            auto array=SafeArrayCreateVector(VT_UNKNOWN,0,found!=nodes.end() ? 1 : 0);
            if (!array) return E_OUTOFMEMORY;
            if (found!=nodes.end()) {
                ComPtr<IRawElementProviderSimple> selected;
                HRESULT hr=element(found->id,selected.GetAddressOf());
                LONG index=0;
                if (SUCCEEDED(hr)) hr=SafeArrayPutElement(array,&index,selected.Get());
                if (FAILED(hr)) { SafeArrayDestroy(array); return hr; }
            }
            *out=array; return S_OK;
        });
    }
    IFACEMETHODIMP get_CanSelectMultiple(BOOL* out) override { *out=FALSE; return S_OK; }
    IFACEMETHODIMP get_IsSelectionRequired(BOOL* out) override { *out=FALSE; return S_OK; }
    IFACEMETHODIMP Select() override {return action(Action::select);}
    IFACEMETHODIMP AddToSelection() override {return Select();}
    IFACEMETHODIMP RemoveFromSelection() override {return UIA_E_INVALIDOPERATION;}
    IFACEMETHODIMP get_IsSelected(BOOL* out) override {return read([&](const Element& e,const auto&){*out=e.selected;return S_OK;});}
    IFACEMETHODIMP get_SelectionContainer(IRawElementProviderSimple** out) override {return read([&](const Element& e,const auto&){return element(e.parent,out);});}
};

void focus_changed(const Window& window,uint64_t id)
{
    if (!window || !UiaClientsAreListening()) return;
    auto focused=Microsoft::WRL::Make<Provider>(window,id);
    if (focused) UiaRaiseAutomationEvent(focused.Get(),UIA_AutomationFocusChangedEventId);
}

LRESULT provider(const Window& window, WPARAM w, LPARAM l) {
    auto root=Microsoft::WRL::Make<Provider>(window,0);
    return root ? UiaReturnRawElementProvider(window->hwnd,w,l,root.Get()) : 0;
}
}

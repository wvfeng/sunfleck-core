#define NOMINMAX
#include "windows_drop.h"
#include <shlobj.h>
#include <wrl.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "utf8_text.h"

namespace win_drop {
using Microsoft::WRL::ComPtr;
static thread_local IDataObject* local_drag = nullptr;
static thread_local ui_drop_data local_data{};
static FORMATETC format(CLIPFORMAT type) { return {type,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL}; }
static CLIPFORMAT object_format() { static const auto f=RegisterClipboardFormatW(L"Sunfleck.LocalObject"); return static_cast<CLIPFORMAT>(f); }

class Target final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDropTarget> {
    HWND hwnd;
    void* owner;
    Receiver receive;
    ComPtr<IDataObject> offered;
    DWORD effect(POINTL point, DWORD allowed, bool deliver) {
        if (!(allowed&DROPEFFECT_COPY) || !offered) return DROPEFFECT_NONE;
        POINT pos{point.x,point.y};
        if (!ScreenToClient(hwnd,&pos)) return DROPEFFECT_NONE;
        for (auto kind : {UI_DROP_OBJECT,UI_DROP_FILES,UI_DROP_TEXT}) {
            auto fmt=format(kind==UI_DROP_TEXT ? CF_UNICODETEXT : kind==UI_DROP_FILES ? CF_HDROP : object_format());
            if (kind==UI_DROP_OBJECT && offered.Get()!=local_drag) continue;
            if (offered->QueryGetData(&fmt)!=S_OK || !receive(owner,pos,kind,nullptr)) continue;
            if (!deliver) return DROPEFFECT_COPY;
            if (kind==UI_DROP_OBJECT) return receive(owner,pos,kind,&local_data) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
            STGMEDIUM medium{};
            if (FAILED(offered->GetData(&fmt,&medium))) continue;
            struct Release { STGMEDIUM& m; ~Release(){ReleaseStgMedium(&m);} } release{medium};
            if (medium.tymed!=TYMED_HGLOBAL || !medium.hGlobal) continue;
            ui_drop_data payload{}; payload.kind=kind;
            std::vector<std::string> strings;
            std::vector<const char*> paths;
            if (kind==UI_DROP_FILES) {
                auto drop=static_cast<HDROP>(medium.hGlobal);
                UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0);
                if (!count) continue;
                for(UINT i=0;i<count;++i) {
                    UINT size=DragQueryFileW(drop,i,nullptr,0);
                    std::wstring path(size+1,L'\0');
                    if (DragQueryFileW(drop,i,path.data(),size+1)!=size) return DROPEFFECT_NONE;
                    path.resize(size); strings.push_back(utf8_text::encode(path));
                }
                for(const auto& path:strings) paths.push_back(path.c_str());
                payload.paths=paths.data(); payload.count=count;
            } else {
                const auto capacity=GlobalSize(medium.hGlobal)/sizeof(wchar_t);
                auto* text=static_cast<const wchar_t*>(GlobalLock(medium.hGlobal));
                if (!text) continue;
                struct Unlock { HGLOBAL h; ~Unlock(){ GlobalUnlock(h); } } unlock{medium.hGlobal};
                auto length=wcsnlen(text,capacity);
                if(length==capacity) continue;
                strings.push_back(utf8_text::encode({text,length}));
                payload.text=strings.front().c_str();
            }
            return receive(owner,pos,kind,&payload) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        return DROPEFFECT_NONE;
    }
    HRESULT update(POINTL p,DWORD* allowed,bool deliver) noexcept {
        try {*allowed=effect(p,*allowed,deliver);return S_OK;}
        catch (...) {*allowed=DROPEFFECT_NONE;return E_FAIL;}
    }
public:
    Target(HWND h,void* o,Receiver r):hwnd(h),owner(o),receive(r){}
    IFACEMETHODIMP DragEnter(IDataObject* data,DWORD,POINTL p,DWORD* allowed) override {offered=data;return update(p,allowed,false);}
    IFACEMETHODIMP DragOver(DWORD,POINTL p,DWORD* allowed) override {return update(p,allowed,false);}
    IFACEMETHODIMP DragLeave() override {offered.Reset();return S_OK;}
    IFACEMETHODIMP Drop(IDataObject* data,DWORD,POINTL p,DWORD* allowed) override {
        offered=data;auto hr=update(p,allowed,true);offered.Reset();return hr;
    }
};

HRESULT attach(HWND window,void* owner,Receiver receiver) {
    auto init=OleInitialize(nullptr);
    if(FAILED(init)) return init;
    auto target=Microsoft::WRL::Make<Target>(window,owner,receiver);
    auto hr=target ? RegisterDragDrop(window,target.Get()) : E_OUTOFMEMORY;
    if(FAILED(hr)) OleUninitialize();
    return hr;
}
void detach(HWND window) { RevokeDragDrop(window); OleUninitialize(); }

ui_result drag(HWND hwnd,ui_app* app,const ui_drop_data& data) {
    if(local_drag) return UI_ERROR_INVALID_STATE;
    ComPtr<IDataObject> object;
    if(FAILED(SHCreateDataObject(nullptr,0,nullptr,nullptr,IID_PPV_ARGS(&object)))) return UI_ERROR_PLATFORM;
    std::wstring content;
    size_t prefix=0;
    if(data.kind==UI_DROP_FILES) {
        if(!data.count || !data.paths) return UI_ERROR_INVALID_ARGUMENT;
        prefix=sizeof(DROPFILES);
        for(unsigned i=0;i<data.count;++i) {
            if(!data.paths[i] || !*data.paths[i]) return UI_ERROR_INVALID_ARGUMENT;
            content+=utf8_text::decode(data.paths[i]); content.push_back(0);
        }
    } else if(data.kind==UI_DROP_TEXT && data.text) content=utf8_text::decode(data.text);
    else if(data.kind!=UI_DROP_OBJECT || data.owner!=app) return UI_ERROR_INVALID_ARGUMENT;
    size_t size=prefix+(content.size()+1)*sizeof(wchar_t);
    auto memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,size);
    if(!memory) return UI_ERROR_OUT_OF_MEMORY;
    auto bytes=static_cast<unsigned char*>(GlobalLock(memory));
    if(!bytes) {GlobalFree(memory);return UI_ERROR_OUT_OF_MEMORY;}
    if(prefix) {auto* header=reinterpret_cast<DROPFILES*>(bytes);header->pFiles=sizeof(DROPFILES);header->fWide=TRUE;}
    memcpy(bytes+prefix,content.c_str(),size-prefix);
    GlobalUnlock(memory);
    auto fmt=format(data.kind==UI_DROP_FILES ? CF_HDROP : data.kind==UI_DROP_TEXT ? CF_UNICODETEXT : object_format());
    STGMEDIUM medium{TYMED_HGLOBAL}; medium.hGlobal=memory;
    if(FAILED(object->SetData(&fmt,&medium,TRUE))) {GlobalFree(memory);return UI_ERROR_PLATFORM;}
    local_data=data; local_drag=object.Get();
    DWORD effect{};
    auto result=SHDoDragDrop(hwnd,object.Get(),nullptr,DROPEFFECT_COPY,&effect);
    local_data={}; local_drag=nullptr;
    return result==DRAGDROP_S_DROP && effect ? UI_OK : FAILED(result) ? UI_ERROR_PLATFORM : UI_CANCELLED;
}
}

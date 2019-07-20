#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <windows.h>
#include <stdint.h>
struct lwin_context {
    uintptr_t hwnd;
    lua_State *L;
    int w;
    int h;
    uintptr_t dc;
};

static struct lwin_context *_ctx;

#define LWIN_TAG &_ctx
#define CLASSNAME L"lua-window"
#define CHECK_CREATE(L) if(!_ctx){luaL_error(L,"must call create before call this!");}
#define IDLE_IDX 1
#define CREATED_IDX 2
#define MESSAGE_IDX 3

static LRESULT _wndproc(HWND hwnd,UINT type,WPARAM wp,LPARAM lp);

static ATOM 
_regclass(HINSTANCE hInst)   
{   
    WNDCLASSW    wc;   
    wc.style=          CS_HREDRAW|CS_VREDRAW;   
    wc.lpfnWndProc=    (WNDPROC)_wndproc;   
    wc.cbClsExtra=     0;   
    wc.cbWndExtra=     0;   
    wc.hInstance=      hInst;   
    wc.hIcon=          NULL;   
    wc.hCursor=        LoadCursor(NULL, IDC_ARROW);   
    wc.hbrBackground=  NULL;   
    wc.lpszMenuName=   NULL;   
    wc.lpszClassName=  CLASSNAME;
    return RegisterClassW(&wc);   
}

static int
lcreate(lua_State *L){
    // title w h    
    const char *title = luaL_checkstring(L,1);
    int w = (int)luaL_checkinteger(L,2);
    int h = (int)luaL_checkinteger(L,3);
    HINSTANCE hInst = GetModuleHandle(0);
    ATOM r = _regclass(hInst);
    if(r == 0){
        luaL_error(L,"register class fail");
    }
    DWORD style=WS_OVERLAPPEDWINDOW&~WS_SIZEBOX&~WS_MAXIMIZEBOX;   
    RECT rect={0,0,w,h};   
    AdjustWindowRect(&rect,style,FALSE);   
    HWND hWnd= CreateWindowW(CLASSNAME,title,   
        style, CW_USEDEFAULT,0,   
        rect.right - rect.left,rect.bottom - rect.top,
        0,0,hInst,0);   
    if(!hWnd){
        luaL_error(L,"create window fail");
    }
    _ctx = malloc(sizeof(*_ctx));
    _ctx->hwnd = (uintptr_t)hWnd;
    _ctx->L = L;
    _ctx->w = 0;
    _ctx->h = 0;
    _ctx->dc = 0;
    lua_newtable(L);
    lua_rawsetp(L,LUA_REGISTRYINDEX,LWIN_TAG);
    return 0;
}


static int
lshow(lua_State *L){
    CHECK_CREATE(L)
    HWND hwnd = (HWND)_ctx->hwnd;
    ShowWindow(hwnd, SW_SHOWDEFAULT);   
    UpdateWindow(hwnd);   
    return 0;
}

static int
lgetdc(lua_State *L){
    CHECK_CREATE(L)
    if(!_ctx->dc){
        _ctx->dc = (uintptr_t)GetDC((HWND)_ctx->hwnd);
    }
    lua_pushinteger(L,_ctx->dc);
    return 1;
}

static int
lgethwnd(lua_State *L){
    CHECK_CREATE(L)
    lua_pushinteger(L,_ctx->hwnd);
    return 1;
}

static int
lgetsize(lua_State *L){
    CHECK_CREATE(L)
    if(!_ctx->w){
        RECT rect;
        GetClientRect((HWND)_ctx->hwnd,&rect);
        _ctx->w = rect.right - rect.left;
        _ctx->h = rect.bottom - rect.top;
    }
    lua_pushinteger(L,_ctx->w);
    lua_pushinteger(L,_ctx->h);
    return 2;
}

static int 
_call(int idx,UINT type,WPARAM wp,LPARAM lp,int *ret){
    lua_State *L = _ctx->L;
    lua_rawgetp(L,LUA_REGISTRYINDEX,LWIN_TAG);
    int t = lua_rawgeti(L,-1,idx);
    if(t == LUA_TFUNCTION){
        lua_pushinteger(L,_ctx->hwnd);
        lua_pushinteger(L,type);
        lua_pushinteger(L,wp);
        lua_pushinteger(L,lp);
        int r = lua_pcall(L,4,1,0);
        if(r == LUA_OK){
            if(lua_type(L,-1) == LUA_TNUMBER){
                *ret = lua_tointeger(L,-1);
                lua_pop(L,2);
                return LUA_TNUMBER;
            }
            else{
                *ret = 0;
                lua_pop(L,2);
                return LUA_TNIL;
            }
        }
        else{
            fprintf(stderr,"call err:%s\n",lua_tostring(L,-1));
            lua_pop(L,2);
            return LUA_TNIL;
        }
    }
    else{
       lua_pop(L,2);
       return LUA_TNIL;
    }
}

static int
lloop(lua_State *L){
    CHECK_CREATE(L)
    MSG msg;
    int ret;
    _call(CREATED_IDX,0,0,0,&ret);
    for (;;) {   
        if(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {   
            if(!GetMessage(&msg,NULL,0,0)) {   
                break;   
            }   
            TranslateMessage(&msg);   
            DispatchMessage(&msg);   
        }  
        _call(IDLE_IDX,0,0,0,&ret);
    }   
}

static LRESULT 
_wndproc(HWND hwnd,UINT type,WPARAM wp,LPARAM lp){
    if(!_ctx) {return DefWindowProc(hwnd,type,wp,lp);}
    int ret;
    int t = _call(MESSAGE_IDX,type,wp,lp,&ret);
    if(type == WM_DESTROY){
        PostQuitMessage(0);
        return 0;
    }
    if(t == LUA_TNIL){
        return DefWindowProc(hwnd,type,wp,lp);
    }
    else{
        return ret;
    }
}

struct enum_pair {
	const char * name;
	lua_Integer value;
};

#define ENUM(prefix, name) { #name, prefix##_##name }

static void
enum_gen(lua_State *L, const char *name, struct enum_pair *enums) {
	int i;
	lua_newtable(L);
	for (i=0;enums[i].name;i++) {
		lua_pushinteger(L, enums[i].value);
		lua_setfield(L, -2, enums[i].name);
	}
	lua_setfield(L, -2, name);
}

static struct enum_pair _wms[] = {
    ENUM(WM,CHAR),
    ENUM(WM,IME_CHAR),
    ENUM(WM,SYSKEYDOWN),
    ENUM(WM,KEYDOWN),
    ENUM(WM,KEYUP),
    ENUM(WM,SYSKEYUP),
    ENUM(WM,MBUTTONDBLCLK),
    ENUM(WM,LBUTTONDBLCLK),
    ENUM(WM,RBUTTONDBLCLK),
    ENUM(WM,LBUTTONDOWN),
    ENUM(WM,RBUTTONDOWN),
    ENUM(WM,MBUTTONDOWN),
    ENUM(WM,MBUTTONUP),
    ENUM(WM,LBUTTONUP),
    ENUM(WM,RBUTTONUP),
    ENUM(WM,MOUSEWHEEL),
    {NULL,0}
};

static struct enum_pair _vks[] = {
    ENUM(VK,TAB),
    ENUM(VK,LEFT),
    ENUM(VK,RIGHT),
    ENUM(VK,UP),
    ENUM(VK,DOWN),
    ENUM(VK,PRIOR),
    ENUM(VK,NEXT),
    ENUM(VK,HOME),
    ENUM(VK,END),
    ENUM(VK,INSERT),
    ENUM(VK,DELETE),
    ENUM(VK,BACK),
    ENUM(VK,SPACE),
    ENUM(VK,RETURN),
    ENUM(VK,ESCAPE),
    ENUM(VK,CONTROL),
    ENUM(VK,SHIFT),
    ENUM(VK,MENU),
    {NULL,0}
};

static int
lgetcursorpos(lua_State *L){
    CHECK_CREATE(L)
    POINT pos;
    GetCursorPos(&pos);
    lua_pushinteger(L,pos.x);
    lua_pushinteger(L,pos.y);
    return 2;
}

static int
lsetcursorpos(lua_State *L){
    CHECK_CREATE(L)
    int x = luaL_checkinteger(L,1);
    int y = luaL_checkinteger(L,2);
    SetCursorPos(x,y);
    return 0;
}

static int
lscreen2client(lua_State *L){
    CHECK_CREATE(L)
    POINT pos;
    pos.x = luaL_checkinteger(L,1);
    pos.y = luaL_checkinteger(L,2);
    ScreenToClient((HWND)_ctx->hwnd,&pos);
    lua_pushinteger(L,pos.x);
    lua_pushinteger(L,pos.y);
    return 2;
}

static int
lclient2screen(lua_State *L){
    CHECK_CREATE(L)
    POINT pos;
    pos.x = luaL_checkinteger(L,1);
    pos.y = luaL_checkinteger(L,2);
    ClientToScreen((HWND)_ctx->hwnd,&pos);
    lua_pushinteger(L,pos.x);
    lua_pushinteger(L,pos.y);
    return 2;
}

static int
lgetkeystate(lua_State *L){
    int vk = luaL_checkinteger(L,1);
    int r = GetKeyState(vk);
    lua_pushinteger(L,r);
    return 1;
}

static int 
lon(lua_State *L){
    CHECK_CREATE(L)
    const char *ev = luaL_checkstring(L,1);
    luaL_checktype(L,2,LUA_TFUNCTION);
    int idx = 0;
    if(strcmp(ev,"create") == 0){idx = CREATED_IDX;}
    else if(strcmp(ev,"idle") == 0){idx = IDLE_IDX;}
    else if(strcmp(ev,"message") == 0){idx = MESSAGE_IDX;}
    else{luaL_error(L,"invalid event type!");}
    lua_rawgetp(L,LUA_REGISTRYINDEX,LWIN_TAG);
    lua_pushvalue(L,2);
    lua_rawseti(L,-2,idx);
    return 0;
}

int 
luaopen_window(lua_State *L){
    luaL_checkversion(L);
    luaL_Reg l[] = {
        {"create",lcreate},
        {"loop",lloop},
        {"show",lshow},
        {"on",lon},
        {"gethwnd",lgethwnd},
        {"getdc",lgetdc},
        {"getsize",lgetsize},
        {"getcursorpos",lgetcursorpos},
        {"setcursorpos",lsetcursorpos},
        {"screen2client",lscreen2client},
        {"client2screen",lclient2screen},
        {"getkeystate",lgetkeystate},
        {NULL,NULL}
    };
    luaL_newlib(L,l);
    enum_gen(L,"wm",_wms);
    enum_gen(L,"vk",_vks);
    return 1;
}
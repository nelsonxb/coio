#include <lua.h>
#include <lauxlib.h>
#include <uv.h>

#include "coio.h"


static luaL_Reg module[] = {
    {"create", coio_loop_create},
    {"run", coio_loop_run},
    {NULL, NULL}
};

int luaopen_coio_loop(lua_State *L)
{
    lua_createtable(L, 0, 2);
    luaL_setfuncs(L, module, 0);
    return 1;
}


int coio_loop_create(lua_State *L)
{
    // Create the event loop
    uv_loop_t *loop = (uv_loop_t *) lua_newuserdata(L, sizeof(uv_loop_t));
    // Create metatable
    luaL_newmetatable(L, COIO_LOOP_TNAME);
    // gc event
    lua_pushcfunction(L, coio_loop__gc);
    lua_setfield(L, -2, "__gc");
    // index event
    lua_createtable(L, 0, 2);
    luaL_setfuncs(L, module + 1 /* Skip "create" */, 0);
    lua_setfield(L, -2, "__index");
    // Set metatable on userdatum
    lua_setmetatable(L, -2);
    // Finally, initialize the event loop
    uv_loop_init(loop);
    return 1;
}


static const int k;
void *const coio_loop_curidx = (void *) &k;
// NOTE: This function is _blocking_.
int coio_loop_run(lua_State *L)
{
    uv_loop_t *loop = (uv_loop_t *) luaL_checkudata(L, 1, COIO_LOOP_TNAME);

    // Hold previous loop on the stack
    lua_pushlightuserdata(L, coio_loop_curidx);
    lua_gettable(L, LUA_REGISTRYINDEX);

    // Create loop data
    lua_createtable(L, 0, 1);
    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "uv_loop");

    // Set this loop as the current
    lua_pushlightuserdata(L, coio_loop_curidx);
    lua_insert(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);

    // Set up starting function
    if (lua_isfunction(L, 2)) {
        // Create async function from normal function
        lua_pushcfunction(L, coio_async);
        lua_pushvalue(L, 2);
        lua_call(L, 1, 1);
    } else {
        // Bring async function up to top of stack
        lua_pushvalue(L, 2);
    }
    // Call async function, setting it up on the
    // event loop.
    lua_call(L, 0, 0);

    // Run event loop
    uv_run(loop, UV_RUN_DEFAULT);

    // Restore the previous loop
    // (or whatever was in that key before).
    lua_pushlightuserdata(L, coio_loop_curidx);
    lua_insert(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);

    return 0;
}


int coio_loop__gc(lua_State *L)
{
    uv_loop_t *loop = (uv_loop_t *) lua_touserdata(L, 1);
    // TODO: Handle UV_EBUSY
    // The way to do that would probably be to error. I don't see any reason
    // that the GC would collect this object if the loop is still running.
    uv_loop_close(loop);
    return 0;
}
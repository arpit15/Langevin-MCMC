#include <iostream>
#include <vector>
#include "commondef.h"
#include "utils.h"

using namespace std;
using namespace chad;

using pathsig = void (*)(const Float*, Float*);
using pathsigder = void (*)(const Float*, Float*, Float*);

int main() {

    auto libpath = std::string(getenv("DPT_LIBPATH"));
    Library lib(libpath, "myfunc");

    Argument x("x", 2);
    auto func =
        BeginFunction("myfunc", {x});
    
    // inputs
    auto xParams = x.GetExprVec();
    // ===========================================
    // function def
    std::vector<CondExprCPtr> ret = CreateCondExprVec(1);

    BooleanCPtr equate =  Eq(xParams[0], Const<ADFloat>(3.f));
    BeginIf( equate, ret);
    {
        SetCondOutput({xParams[0]*xParams[0]});
       
    }
    BeginElse();
    {
        // SetCondOutput({Const<ADFloat>(9.f)});
        SetCondOutput({xParams[0]*100.f});
    }
    EndIf();

    // ADFloat y = xParams[0]*xParams[0];

    ADFloat y(ret[0]);
    // ===================================
    EndFunction({{"y", {y}}});

    lib.RegisterFunc(func);
    lib.RegisterFuncDerv(func, xParams, y, false);
    cout << "Registered func name : " << func->name << endl;

    lib.Link();

    pathsig f = (pathsig) lib.GetFunc("myfunc");
    pathsigder fder = (pathsigder) lib.GetFuncDerv("myfunc");

    // input
    const Float xIn[] = {2.f, 5.f};
    Float yout[1];
    cout << "Forward!" << endl;
    f(&xIn[0], &yout[0]);
    cout << "y=" << yout[0] << endl;

    Float grad[2], hess[4];
    cout << "Derivative !" << endl;
    fder(&xIn[0], &grad[0], &hess[0]);
    cout << "Returned!" << endl;
    cout << "Derivative : " << grad[0] << ", " << grad[1] << endl;

    return 0;
}
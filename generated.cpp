#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using std::cout;
using std::endl;

struct E {
};

E* E__ctor();

E* E__ctor() {
    E* obj = new E();
    return obj;
}

int main() {
    std::vector<std::string> a;
    int bool17 = 0;
    int bool18 = 0;
    int bool20 = 0;
    int const0 = 0;
    int const1 = 0;
    int const10 = 0;
    int const12 = 0;
    int const14 = 0;
    int const23 = 0;
    int const24 = 0;
    int const26 = 0;
    int const27 = 0;
    int const3 = 0;
    int const5 = 0;
    int const6 = 0;
    int const9 = 0;
    int tmp11 = 0;
    int tmp13 = 0;
    int tmp15 = 0;
    int tmp16 = 0;
    bool tmp19 = false;
    int tmp2 = 0;
    bool tmp21 = false;
    bool tmp22 = false;
    bool tmp25 = false;
    bool tmp28 = false;
    bool tmp29 = false;
    bool tmp30 = false;
    int tmp4 = 0;
    int tmp7 = 0;
    int tmp8 = 0;
    cout << std::boolalpha;
    goto E__main_entry_0;

E__main_entry_0:
    const0 = 4;
    const1 = 2;
    tmp2 = const0 * const1;
    const3 = 10;
    tmp4 = tmp2 + const3;
    const5 = 2;
    const6 = 6;
    tmp7 = const5 * const6;
    tmp8 = tmp4 - tmp7;
    const9 = 4;
    const10 = 1;
    tmp11 = const9 - const10;
    const12 = 2;
    tmp13 = tmp11 * const12;
    const14 = 2;
    tmp15 = tmp13 / const14;
    tmp16 = tmp8 + tmp15;
    cout << tmp16 << endl;
    bool17 = 1;
    bool18 = 1;
    tmp19 = (!bool18);
    bool20 = 0;
    tmp21 = (tmp19 && bool20);
    tmp22 = (bool17 == tmp21);
    const23 = 10;
    const24 = 1;
    tmp25 = (const23 > const24);
    const26 = 1;
    const27 = 10;
    tmp28 = (const26 < const27);
    tmp29 = (tmp25 && tmp28);
    tmp30 = (tmp22 || tmp29);
    cout << tmp30 << endl;
    return 0;
}


#include "gbits/base/flags.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

enum BasicEnum {
  kBasicEnum_Zero,
  kBasicEnum_One,
  kBasicEnum_Two,
  kBasicEnum_Three,
  kBasicEnum_Big = 63,
};

enum SizedEnum : int8_t {
  kSizedEnum_Zero,
  kSizedEnum_One,
  kSizedEnum_Two,
  kSizedEnum_Three,
  kSizedEnum_Big = 63,
};

enum class ClassEnum : int8_t {
  kZero,
  kOne,
  kTwo,
  kThree,
  kBig = 63,
};

// Helpers to test parameter passing and implicit conversion.
Flags<BasicEnum> BasicIdentity(Flags<BasicEnum> flags) { return flags; }
Flags<SizedEnum> SizedIdentity(Flags<SizedEnum> flags) { return flags; }
Flags<ClassEnum> ClassIdentity(Flags<ClassEnum> flags) { return flags; }

// Static assert is used to ensure constexpr-ness.
static_assert(Flags<BasicEnum>().IsEmpty(),
              "BasicEnum default flags not empty");
static_assert(Flags<BasicEnum>().GetMask() == 0,
              "BasicEnum default mask is not zero");
static_assert(Flags<BasicEnum>(1).GetMask() == 1, "BasicEnum 1 is not 1");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero).GetMask() == 1,
              "BasicEnum Zero is not 1");
static_assert(Flags<BasicEnum>(kBasicEnum_Big).GetMask() == 1ULL << 63,
              "BasicEnum Big is not 1 << 63");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero).IsSet(kBasicEnum_Zero),
              "BasicEnum Zero does not have Zero set");
static_assert(!Flags<BasicEnum>(kBasicEnum_Zero).IsSet(kBasicEnum_One),
              "BasicEnum Zero has One set");
static_assert(Flags<BasicEnum>({}).GetMask() == 0, "BasicEnum {} is not 0");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero}).GetMask() == 1,
              "BasicEnum {kBasicEnum_Zero}  is not 1");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}).GetMask() ==
                  3,
              "BasicEnum {Zero,One} is not 3");
static_assert(
    Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}).IsSet(kBasicEnum_One),
    "BasicEnum {Zero,One} does not have One set");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One,
                                kBasicEnum_Two})
                  .IsSet({kBasicEnum_Zero, kBasicEnum_Two}),
              "BasicEnum {Zero,One,Two} does not have {Zero,Two} set");
static_assert(!Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One})
                   .IsSet({kBasicEnum_Zero, kBasicEnum_Two}),
              "BasicEnum {Zero,One} does have {Zero,Two} set");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One})
                  .Intersects({kBasicEnum_Zero, kBasicEnum_Two}),
              "BasicEnum {Zero,One} does not intersect {Zero,Two}");
static_assert(!Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One})
                   .Intersects({kBasicEnum_Two, kBasicEnum_Three}),
              "BasicEnum {Zero,One} intersects {Two, Three}");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero) ==
                  Flags<BasicEnum>(kBasicEnum_Zero),
              "BasicEnum Zero is not equal to Zero");
static_assert(!(Flags<BasicEnum>(kBasicEnum_Zero) ==
                Flags<BasicEnum>(kBasicEnum_One)),
              "BasicEnum Zero is equal to One");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero) !=
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum Zero is equal to One");
static_assert(!(Flags<BasicEnum>(kBasicEnum_Zero) !=
                Flags<BasicEnum>(kBasicEnum_Zero)),
              "BasicEnum Zero is not equal to Zero");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero) <
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum Zero is not less than One");
static_assert(!(Flags<BasicEnum>(kBasicEnum_Zero) <
                Flags<BasicEnum>(kBasicEnum_Zero)),
              "BasicEnum Zero is less than Zero");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero) <=
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum Zero is not less or equal to One");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero) <=
                  Flags<BasicEnum>(kBasicEnum_Zero),
              "BasicEnum Zero is not less or equal to Zero");
static_assert(!(Flags<BasicEnum>(kBasicEnum_One) <=
                Flags<BasicEnum>(kBasicEnum_Zero)),
              "BasicEnum One is less or equal to Zero");
static_assert(Flags<BasicEnum>(kBasicEnum_One) >
                  Flags<BasicEnum>(kBasicEnum_Zero),
              "BasicEnum One is not greater than Zero");
static_assert(!(Flags<BasicEnum>(kBasicEnum_One) >
                Flags<BasicEnum>(kBasicEnum_One)),
              "BasicEnum One is greater than One");
static_assert(Flags<BasicEnum>(kBasicEnum_One) >=
                  Flags<BasicEnum>(kBasicEnum_Zero),
              "BasicEnum One is not greater or equal to Zero");
static_assert(Flags<BasicEnum>(kBasicEnum_One) >=
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum One is not greater or equal to One");
static_assert(!(Flags<BasicEnum>(kBasicEnum_Zero) >=
                Flags<BasicEnum>(kBasicEnum_One)),
              "BasicEnum Zero is greater or equal to One");
static_assert(Flags<BasicEnum>(kBasicEnum_Zero) +
                      Flags<BasicEnum>(kBasicEnum_One) ==
                  Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
              "BasicEnum Zero + One is not equal to {Zero, One}");
static_assert(
    Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}) +
            Flags<BasicEnum>({kBasicEnum_One, kBasicEnum_Two}) ==
        Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One, kBasicEnum_Two}),
    "BasicEnum {Zero,One} + {One,Two} is not equal to {Zero,One,Two}");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}) -
                      Flags<BasicEnum>(kBasicEnum_Zero) ==
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum {Zero,One} - Zero is not equal to One");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}) -
                      Flags<BasicEnum>(kBasicEnum_Zero) ==
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum {Zero,One} - Zero is not equal to One");
static_assert(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}) -
                      Flags<BasicEnum>(kBasicEnum_Two) ==
                  Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
              "BasicEnum {Zero,One} - Two is not equal to {Zero,One}");
static_assert(Union(kBasicEnum_Zero, Flags<BasicEnum>(kBasicEnum_One)) ==
                  Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
              "BasicEnum Zero union One is not equal to {Zero, One}");
static_assert(
    Union(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
          Flags<BasicEnum>({kBasicEnum_One, kBasicEnum_Two})) ==
        Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One, kBasicEnum_Two}),
    "BasicEnum {Zero,One} union {One,Two} is not equal to {Zero,One,Two}");
static_assert((Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}) -
               Flags<BasicEnum>(kBasicEnum_Zero)) ==
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum {Zero,One} remove Zero is not equal to One");
static_assert((Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}) -
               Flags<BasicEnum>(kBasicEnum_Two)) ==
                  Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
              "BasicEnum {Zero,One} remove Two is not equal to {Zero,One}");
static_assert(Intersect(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
                        Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_Two})) ==
                  Flags<BasicEnum>(kBasicEnum_Zero),
              "BasicEnum {Zero,One} intersect {Zero,Two} is not equal to Zero");
static_assert(Intersect(Flags<BasicEnum>({kBasicEnum_Zero, kBasicEnum_One}),
                        Flags<BasicEnum>(kBasicEnum_Two))
                  .IsEmpty(),
              "BasicEnum {Zero,One} intersect Two is not empty");
static_assert(Flags<BasicEnum>({kBasicEnum_One}) ==
                  Flags<BasicEnum>(kBasicEnum_One),
              "BasicEnum {One} is not equal to (One)");
static_assert(Flags<BasicEnum>({kBasicEnum_One, kBasicEnum_Two}) ==
                  Flags<BasicEnum>(kBasicEnum_One, kBasicEnum_Two),
              "BasicEnum {One,Two} is not equal to (One,Two)");
static_assert(
    Flags<BasicEnum>({kBasicEnum_One, kBasicEnum_Two, kBasicEnum_Three}) ==
        Flags<BasicEnum>(kBasicEnum_One, kBasicEnum_Two, kBasicEnum_Three),
    "BasicEnum {One,Two,Three} is not equal to (One,Two,Three)");
static_assert(
    Flags<BasicEnum>({Flags<BasicEnum>{kBasicEnum_Zero, kBasicEnum_One},
                      kBasicEnum_Two,
                      Flags<BasicEnum>{kBasicEnum_Three, kBasicEnum_Big}}) ==
        Flags<BasicEnum>(Flags<BasicEnum>{kBasicEnum_Zero, kBasicEnum_One},
                         kBasicEnum_Two,
                         Flags<BasicEnum>{kBasicEnum_Three, kBasicEnum_Big}),
    "BasicEnum {{Zero,One},Two,{Three,Big}} is not equal to "
    "({Zero,One},Two,{Three,Big})");

static_assert(Flags<SizedEnum>().IsEmpty(),
              "SizedEnum default flags not empty");
static_assert(Flags<SizedEnum>().GetMask() == 0,
              "SizedEnum default mask is not zero");
static_assert(Flags<SizedEnum>(1).GetMask() == 1, "SizedEnum 1 is not 1");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero).GetMask() == 1,
              "SizedEnum Zero is not 1");
static_assert(Flags<SizedEnum>(kSizedEnum_Big).GetMask() == 1ULL << 63,
              "SizedEnum Big is not 1 << 63");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero).IsSet(kSizedEnum_Zero),
              "SizedEnum Zero does not have Zero set");
static_assert(!Flags<SizedEnum>(kSizedEnum_Zero).IsSet(kSizedEnum_One),
              "SizedEnum Zero has One set");
static_assert(Flags<SizedEnum>({}).GetMask() == 0, "SizedEnum {} is not 0");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero}).GetMask() == 1,
              "SizedEnum {kSizedEnum_Zero}  is not 1");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}).GetMask() ==
                  3,
              "SizedEnum {Zero,One} is not 3");
static_assert(
    Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}).IsSet(kSizedEnum_One),
    "SizedEnum {Zero,One} does not have One set");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One,
                                kSizedEnum_Two})
                  .IsSet({kSizedEnum_Zero, kSizedEnum_Two}),
              "SizedEnum {Zero,One,Two} does not have {Zero,Two} set");
static_assert(!Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One})
                   .IsSet({kSizedEnum_Zero, kSizedEnum_Two}),
              "SizedEnum {Zero,One} does have {Zero,Two} set");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One})
                  .Intersects({kSizedEnum_Zero, kSizedEnum_Two}),
              "SizedEnum {Zero,One} does not intersect {Zero,Two}");
static_assert(!Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One})
                   .Intersects({kSizedEnum_Two, kSizedEnum_Three}),
              "SizedEnum {Zero,One} intersects {Two, Three}");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero) ==
                  Flags<SizedEnum>(kSizedEnum_Zero),
              "SizedEnum Zero is not equal to Zero");
static_assert(!(Flags<SizedEnum>(kSizedEnum_Zero) ==
                Flags<SizedEnum>(kSizedEnum_One)),
              "SizedEnum Zero is equal to One");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero) !=
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum Zero is equal to One");
static_assert(!(Flags<SizedEnum>(kSizedEnum_Zero) !=
                Flags<SizedEnum>(kSizedEnum_Zero)),
              "SizedEnum Zero is not equal to Zero");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero) <
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum Zero is not less than One");
static_assert(!(Flags<SizedEnum>(kSizedEnum_Zero) <
                Flags<SizedEnum>(kSizedEnum_Zero)),
              "SizedEnum Zero is less than Zero");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero) <=
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum Zero is not less or equal to One");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero) <=
                  Flags<SizedEnum>(kSizedEnum_Zero),
              "SizedEnum Zero is not less or equal to Zero");
static_assert(!(Flags<SizedEnum>(kSizedEnum_One) <=
                Flags<SizedEnum>(kSizedEnum_Zero)),
              "SizedEnum One is less or equal to Zero");
static_assert(Flags<SizedEnum>(kSizedEnum_One) >
                  Flags<SizedEnum>(kSizedEnum_Zero),
              "SizedEnum One is not greater than Zero");
static_assert(!(Flags<SizedEnum>(kSizedEnum_One) >
                Flags<SizedEnum>(kSizedEnum_One)),
              "SizedEnum One is greater than One");
static_assert(Flags<SizedEnum>(kSizedEnum_One) >=
                  Flags<SizedEnum>(kSizedEnum_Zero),
              "SizedEnum One is not greater or equal to Zero");
static_assert(Flags<SizedEnum>(kSizedEnum_One) >=
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum One is not greater or equal to One");
static_assert(!(Flags<SizedEnum>(kSizedEnum_Zero) >=
                Flags<SizedEnum>(kSizedEnum_One)),
              "SizedEnum Zero is greater or equal to One");
static_assert(Flags<SizedEnum>(kSizedEnum_Zero) +
                      Flags<SizedEnum>(kSizedEnum_One) ==
                  Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
              "SizedEnum Zero + One is not equal to {Zero, One}");
static_assert(
    Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}) +
            Flags<SizedEnum>({kSizedEnum_One, kSizedEnum_Two}) ==
        Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One, kSizedEnum_Two}),
    "SizedEnum {Zero,One} + {One,Two} is not equal to {Zero,One,Two}");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}) -
                      Flags<SizedEnum>(kSizedEnum_Zero) ==
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum {Zero,One} - Zero is not equal to One");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}) -
                      Flags<SizedEnum>(kSizedEnum_Zero) ==
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum {Zero,One} - Zero is not equal to One");
static_assert(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}) -
                      Flags<SizedEnum>(kSizedEnum_Two) ==
                  Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
              "SizedEnum {Zero,One} - Two is not equal to {Zero,One}");
static_assert(Union(kSizedEnum_Zero, Flags<SizedEnum>(kSizedEnum_One)) ==
                  Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
              "SizedEnum Zero union One is not equal to {Zero, One}");
static_assert(
    Union(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
          Flags<SizedEnum>({kSizedEnum_One, kSizedEnum_Two})) ==
        Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One, kSizedEnum_Two}),
    "SizedEnum {Zero,One} union {One,Two} is not equal to {Zero,One,Two}");
static_assert((Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}) -
               Flags<SizedEnum>(kSizedEnum_Zero)) ==
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum {Zero,One} remove Zero is not equal to One");
static_assert((Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}) -
               Flags<SizedEnum>(kSizedEnum_Two)) ==
                  Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
              "SizedEnum {Zero,One} remove Two is not equal to {Zero,One}");
static_assert(Intersect(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
                        Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_Two})) ==
                  Flags<SizedEnum>(kSizedEnum_Zero),
              "SizedEnum {Zero,One} intersect {Zero,Two} is not equal to Zero");
static_assert(Intersect(Flags<SizedEnum>({kSizedEnum_Zero, kSizedEnum_One}),
                        Flags<SizedEnum>(kSizedEnum_Two))
                  .IsEmpty(),
              "SizedEnum {Zero,One} intersect Two is not empty");
static_assert(Flags<SizedEnum>({kSizedEnum_One}) ==
                  Flags<SizedEnum>(kSizedEnum_One),
              "SizedEnum {One} is not equal to (One)");
static_assert(Flags<SizedEnum>({kSizedEnum_One, kSizedEnum_Two}) ==
                  Flags<SizedEnum>(kSizedEnum_One, kSizedEnum_Two),
              "SizedEnum {One,Two} is not equal to (One,Two)");
static_assert(
    Flags<SizedEnum>({kSizedEnum_One, kSizedEnum_Two, kSizedEnum_Three}) ==
        Flags<SizedEnum>(kSizedEnum_One, kSizedEnum_Two, kSizedEnum_Three),
    "SizedEnum {One,Two,Three} is not equal to (One,Two,Three)");
static_assert(
    Flags<SizedEnum>({Flags<SizedEnum>{kSizedEnum_Zero, kSizedEnum_One},
                      kSizedEnum_Two,
                      Flags<SizedEnum>{kSizedEnum_Three, kSizedEnum_Big}}) ==
        Flags<SizedEnum>(Flags<SizedEnum>{kSizedEnum_Zero, kSizedEnum_One},
                         kSizedEnum_Two,
                         Flags<SizedEnum>{kSizedEnum_Three, kSizedEnum_Big}),
    "SizedEnum {{Zero,One},Two,{Three,Big}} is not equal to "
    "({Zero,One},Two,{Three,Big})");

static_assert(Flags<ClassEnum>().IsEmpty(),
              "ClassEnum default flags not empty");
static_assert(Flags<ClassEnum>().GetMask() == 0,
              "ClassEnum default mask is not zero");
static_assert(Flags<ClassEnum>(1).GetMask() == 1, "ClassEnum 1 is not 1");
static_assert(Flags<ClassEnum>(ClassEnum::kZero).GetMask() == 1,
              "ClassEnum Zero is not 1");
static_assert(Flags<ClassEnum>(ClassEnum::kBig).GetMask() == 1ULL << 63,
              "ClassEnum Big is not 1 << 63");
static_assert(Flags<ClassEnum>(ClassEnum::kZero).IsSet(ClassEnum::kZero),
              "ClassEnum Zero does not have Zero set");
static_assert(!Flags<ClassEnum>(ClassEnum::kZero).IsSet(ClassEnum::kOne),
              "ClassEnum Zero has One set");
static_assert(Flags<ClassEnum>({}).GetMask() == 0, "ClassEnum {} is not 0");
static_assert(Flags<ClassEnum>({ClassEnum::kZero}).GetMask() == 1,
              "ClassEnum {ClassEnum::kZero}  is not 1");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}).GetMask() ==
                  3,
              "ClassEnum {Zero,One} is not 3");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne})
                  .IsSet(ClassEnum::kOne),
              "ClassEnum {Zero,One} does not have One set");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne,
                                ClassEnum::kTwo})
                  .IsSet({ClassEnum::kZero, ClassEnum::kTwo}),
              "ClassEnum {Zero,One,Two} does not have {Zero,Two} set");
static_assert(!Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne})
                   .IsSet({ClassEnum::kZero, ClassEnum::kTwo}),
              "ClassEnum {Zero,One} does have {Zero,Two} set");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne})
                  .Intersects({ClassEnum::kZero, ClassEnum::kTwo}),
              "ClassEnum {Zero,One} does not intersect {Zero,Two}");
static_assert(!Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne})
                   .Intersects({ClassEnum::kTwo, ClassEnum::kThree}),
              "ClassEnum {Zero,One} intersects {Two, Three}");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) ==
                  Flags<ClassEnum>(ClassEnum::kZero),
              "ClassEnum Zero is not equal to Zero");
static_assert(!(Flags<ClassEnum>(ClassEnum::kZero) ==
                Flags<ClassEnum>(ClassEnum::kOne)),
              "ClassEnum Zero is equal to One");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) == ClassEnum::kZero,
              "ClassEnum Zero is not equal to Zero");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) !=
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum Zero is equal to One");
static_assert(!(Flags<ClassEnum>(ClassEnum::kZero) !=
                Flags<ClassEnum>(ClassEnum::kZero)),
              "ClassEnum Zero is not equal to Zero");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) != ClassEnum::kOne,
              "ClassEnum Zero is equal to One");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) <
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum Zero is not less than One");
static_assert(!(Flags<ClassEnum>(ClassEnum::kZero) <
                Flags<ClassEnum>(ClassEnum::kZero)),
              "ClassEnum Zero is less than Zero");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) <=
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum Zero is not less or equal to One");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) <=
                  Flags<ClassEnum>(ClassEnum::kZero),
              "ClassEnum Zero is not less or equal to Zero");
static_assert(!(Flags<ClassEnum>(ClassEnum::kOne) <=
                Flags<ClassEnum>(ClassEnum::kZero)),
              "ClassEnum One is less or equal to Zero");
static_assert(Flags<ClassEnum>(ClassEnum::kOne) >
                  Flags<ClassEnum>(ClassEnum::kZero),
              "ClassEnum One is not greater than Zero");
static_assert(!(Flags<ClassEnum>(ClassEnum::kOne) >
                Flags<ClassEnum>(ClassEnum::kOne)),
              "ClassEnum One is greater than One");
static_assert(Flags<ClassEnum>(ClassEnum::kOne) >=
                  Flags<ClassEnum>(ClassEnum::kZero),
              "ClassEnum One is not greater or equal to Zero");
static_assert(Flags<ClassEnum>(ClassEnum::kOne) >=
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum One is not greater or equal to One");
static_assert(!(Flags<ClassEnum>(ClassEnum::kZero) >=
                Flags<ClassEnum>(ClassEnum::kOne)),
              "ClassEnum Zero is greater or equal to One");
static_assert(Flags<ClassEnum>(ClassEnum::kZero) +
                      Flags<ClassEnum>(ClassEnum::kOne) ==
                  Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
              "ClassEnum Zero + One is not equal to {Zero, One}");
static_assert(
    Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}) +
            Flags<ClassEnum>({ClassEnum::kOne, ClassEnum::kTwo}) ==
        Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne, ClassEnum::kTwo}),
    "ClassEnum {Zero,One} + {One,Two} is not equal to {Zero,One,Two}");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}) -
                      Flags<ClassEnum>(ClassEnum::kZero) ==
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum {Zero,One} - Zero is not equal to One");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}) -
                      Flags<ClassEnum>(ClassEnum::kZero) ==
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum {Zero,One} - Zero is not equal to One");
static_assert(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}) -
                      Flags<ClassEnum>(ClassEnum::kTwo) ==
                  Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
              "ClassEnum {Zero,One} - Two is not equal to {Zero,One}");
static_assert(Union(Flags<ClassEnum>(ClassEnum::kZero),
                    Flags<ClassEnum>(ClassEnum::kOne)) ==
                  Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
              "ClassEnum Zero union One is not equal to {Zero, One}");
static_assert(
    Union(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
          Flags<ClassEnum>({ClassEnum::kOne, ClassEnum::kTwo})) ==
        Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne, ClassEnum::kTwo}),
    "ClassEnum {Zero,One} union {One,Two} is not equal to {Zero,One,Two}");
static_assert((Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}) -
               Flags<ClassEnum>(ClassEnum::kZero)) ==
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum {Zero,One} remove Zero is not equal to One");
static_assert((Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}) -
               Flags<ClassEnum>(ClassEnum::kTwo)) ==
                  Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
              "ClassEnum {Zero,One} remove Two is not equal to {Zero,One}");
static_assert(Intersect(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
                        Flags<ClassEnum>({ClassEnum::kZero,
                                          ClassEnum::kTwo})) ==
                  Flags<ClassEnum>(ClassEnum::kZero),
              "ClassEnum {Zero,One} intersect {Zero,Two} is not equal to Zero");
static_assert(Intersect(Flags<ClassEnum>({ClassEnum::kZero, ClassEnum::kOne}),
                        Flags<ClassEnum>(ClassEnum::kTwo))
                  .IsEmpty(),
              "ClassEnum {Zero,One} intersect Two is not empty");
static_assert(Flags<ClassEnum>({ClassEnum::kOne}) ==
                  Flags<ClassEnum>(ClassEnum::kOne),
              "ClassEnum {One} is not equal to (One)");
static_assert(Flags<ClassEnum>({ClassEnum::kOne, ClassEnum::kTwo}) ==
                  Flags<ClassEnum>(ClassEnum::kOne, ClassEnum::kTwo),
              "ClassEnum {One,Two} is not equal to (One,Two)");
static_assert(
    Flags<ClassEnum>({ClassEnum::kOne, ClassEnum::kTwo, ClassEnum::kThree}) ==
        Flags<ClassEnum>(ClassEnum::kOne, ClassEnum::kTwo, ClassEnum::kThree),
    "ClassEnum {One,Two,Three} is not equal to (One,Two,Three)");
static_assert(
    Flags<ClassEnum>({Flags<ClassEnum>{ClassEnum::kZero, ClassEnum::kOne},
                      ClassEnum::kTwo,
                      Flags<ClassEnum>{ClassEnum::kThree, ClassEnum::kBig}}) ==
        Flags<ClassEnum>(Flags<ClassEnum>{ClassEnum::kZero, ClassEnum::kOne},
                         ClassEnum::kTwo,
                         Flags<ClassEnum>{ClassEnum::kThree, ClassEnum::kBig}),
    "ClassEnum {{Zero,One},Two,{Three,Big}} is not equal to "
    "({Zero,One},Two,{Three,Big})");

TEST(FlagsTest, BasicImplicitParameterConversions) {
  EXPECT_EQ(BasicIdentity({}), Flags<BasicEnum>({}));
  EXPECT_EQ(BasicIdentity(kBasicEnum_One), Flags<BasicEnum>(kBasicEnum_One));
  EXPECT_EQ(BasicIdentity({kBasicEnum_One}),
            Flags<BasicEnum>({kBasicEnum_One}));
  EXPECT_EQ(BasicIdentity({kBasicEnum_One, kBasicEnum_Two}),
            Flags<BasicEnum>(kBasicEnum_One, kBasicEnum_Two));
}

TEST(FlagsTest, BasicSet) {
  Flags<BasicEnum> flags;
  flags.Set(kBasicEnum_Zero);
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_Zero));
  flags.Set({kBasicEnum_One, kBasicEnum_Two});
  EXPECT_EQ(flags,
            Flags<BasicEnum>(kBasicEnum_Zero, kBasicEnum_One, kBasicEnum_Two));
  flags.Set({kBasicEnum_One, kBasicEnum_Three});
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_Zero, kBasicEnum_One,
                                    kBasicEnum_Two, kBasicEnum_Three));
}

TEST(FlagsTest, BasicClear) {
  Flags<BasicEnum> flags(kBasicEnum_Zero);
  flags.Clear();
  EXPECT_TRUE(flags.IsEmpty());
  flags.Set({kBasicEnum_One, kBasicEnum_Two});
  flags.Clear(kBasicEnum_One);
  EXPECT_EQ(flags, kBasicEnum_Two);
}

TEST(FlagsTest, BasicAssign) {
  Flags<BasicEnum> flags;
  flags = kBasicEnum_Zero;
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_Zero));
  flags = {kBasicEnum_One, kBasicEnum_Two};
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_One, kBasicEnum_Two));
}

TEST(FlagsTest, BasicAddAssign) {
  Flags<BasicEnum> flags;
  flags += kBasicEnum_Zero;
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_Zero));
  flags += {kBasicEnum_One, kBasicEnum_Two};
  EXPECT_EQ(flags,
            Flags<BasicEnum>(kBasicEnum_Zero, kBasicEnum_One, kBasicEnum_Two));
  flags += {kBasicEnum_One, kBasicEnum_Three};
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_Zero, kBasicEnum_One,
                                    kBasicEnum_Two, kBasicEnum_Three));
  flags += {};
  EXPECT_EQ(flags, Flags<BasicEnum>(kBasicEnum_Zero, kBasicEnum_One,
                                    kBasicEnum_Two, kBasicEnum_Three));
}

TEST(FlagsTest, BasicSubAssign) {
  Flags<BasicEnum> flags(kBasicEnum_Zero);
  flags -= kBasicEnum_Zero;
  EXPECT_TRUE(flags.IsEmpty());
  flags.Set({kBasicEnum_One, kBasicEnum_Two, kBasicEnum_Three});
  flags -= {kBasicEnum_One, kBasicEnum_Three};
  EXPECT_EQ(flags, kBasicEnum_Two);
  flags -= {};
  EXPECT_EQ(flags, kBasicEnum_Two);
}

TEST(FlagsTest, SizedImplicitParameterConversions) {
  EXPECT_EQ(SizedIdentity({}), Flags<SizedEnum>({}));
  EXPECT_EQ(SizedIdentity(kSizedEnum_One), Flags<SizedEnum>(kSizedEnum_One));
  EXPECT_EQ(SizedIdentity({kSizedEnum_One}),
            Flags<SizedEnum>({kSizedEnum_One}));
  EXPECT_EQ(SizedIdentity({kSizedEnum_One, kSizedEnum_Two}),
            Flags<SizedEnum>(kSizedEnum_One, kSizedEnum_Two));
}

TEST(FlagsTest, SizedSet) {
  Flags<SizedEnum> flags;
  flags.Set(kSizedEnum_Zero);
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_Zero));
  flags.Set({kSizedEnum_One, kSizedEnum_Two});
  EXPECT_EQ(flags,
            Flags<SizedEnum>(kSizedEnum_Zero, kSizedEnum_One, kSizedEnum_Two));
  flags.Set({kSizedEnum_One, kSizedEnum_Three});
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_Zero, kSizedEnum_One,
                                    kSizedEnum_Two, kSizedEnum_Three));
}

TEST(FlagsTest, SizedClear) {
  Flags<SizedEnum> flags(kSizedEnum_Zero);
  flags.Clear();
  EXPECT_TRUE(flags.IsEmpty());
  flags.Set({kSizedEnum_One, kSizedEnum_Two});
  flags.Clear(kSizedEnum_One);
  EXPECT_EQ(flags, kSizedEnum_Two);
}

TEST(FlagsTest, SizedAssign) {
  Flags<SizedEnum> flags;
  flags = kSizedEnum_Zero;
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_Zero));
  flags = {kSizedEnum_One, kSizedEnum_Two};
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_One, kSizedEnum_Two));
}

TEST(FlagsTest, SizedAddAssign) {
  Flags<SizedEnum> flags;
  flags += kSizedEnum_Zero;
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_Zero));
  flags += {kSizedEnum_One, kSizedEnum_Two};
  EXPECT_EQ(flags,
            Flags<SizedEnum>(kSizedEnum_Zero, kSizedEnum_One, kSizedEnum_Two));
  flags += {kSizedEnum_One, kSizedEnum_Three};
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_Zero, kSizedEnum_One,
                                    kSizedEnum_Two, kSizedEnum_Three));
  flags += {};
  EXPECT_EQ(flags, Flags<SizedEnum>(kSizedEnum_Zero, kSizedEnum_One,
                                    kSizedEnum_Two, kSizedEnum_Three));
}

TEST(FlagsTest, SizedSubAssign) {
  Flags<SizedEnum> flags(kSizedEnum_Zero);
  flags -= kSizedEnum_Zero;
  EXPECT_TRUE(flags.IsEmpty());
  flags.Set({kSizedEnum_One, kSizedEnum_Two, kSizedEnum_Three});
  flags -= {kSizedEnum_One, kSizedEnum_Three};
  EXPECT_EQ(flags, kSizedEnum_Two);
  flags -= {};
  EXPECT_EQ(flags, kSizedEnum_Two);
}

TEST(FlagsTest, ClassImplicitParameterConversions) {
  EXPECT_EQ(ClassIdentity({}), Flags<ClassEnum>({}));
  EXPECT_EQ(ClassIdentity(ClassEnum::kOne), Flags<ClassEnum>(ClassEnum::kOne));
  EXPECT_EQ(ClassIdentity({ClassEnum::kOne}),
            Flags<ClassEnum>({ClassEnum::kOne}));
  EXPECT_EQ(ClassIdentity({ClassEnum::kOne, ClassEnum::kTwo}),
            Flags<ClassEnum>(ClassEnum::kOne, ClassEnum::kTwo));
}

TEST(FlagsTest, ClassSet) {
  Flags<ClassEnum> flags;
  flags.Set(ClassEnum::kZero);
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero));
  flags.Set({ClassEnum::kOne, ClassEnum::kTwo});
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero, ClassEnum::kOne,
                                    ClassEnum::kTwo));
  flags.Set({ClassEnum::kOne, ClassEnum::kThree});
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero, ClassEnum::kOne,
                                    ClassEnum::kTwo, ClassEnum::kThree));
}

TEST(FlagsTest, ClassClear) {
  Flags<ClassEnum> flags(ClassEnum::kZero);
  flags.Clear();
  EXPECT_TRUE(flags.IsEmpty());
  flags.Set({ClassEnum::kOne, ClassEnum::kTwo});
  flags.Clear(ClassEnum::kOne);
  EXPECT_EQ(flags, ClassEnum::kTwo);
}

TEST(FlagsTest, ClassAssign) {
  Flags<ClassEnum> flags;
  flags = ClassEnum::kZero;
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero));
  flags = {ClassEnum::kOne, ClassEnum::kTwo};
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kOne, ClassEnum::kTwo));
}

TEST(FlagsTest, ClassAddAssign) {
  Flags<ClassEnum> flags;
  flags += ClassEnum::kZero;
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero));
  flags += {ClassEnum::kOne, ClassEnum::kTwo};
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero, ClassEnum::kOne,
                                    ClassEnum::kTwo));
  flags += {ClassEnum::kOne, ClassEnum::kThree};
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero, ClassEnum::kOne,
                                    ClassEnum::kTwo, ClassEnum::kThree));
  flags += {};
  EXPECT_EQ(flags, Flags<ClassEnum>(ClassEnum::kZero, ClassEnum::kOne,
                                    ClassEnum::kTwo, ClassEnum::kThree));
}

TEST(FlagsTest, ClassSubAssign) {
  Flags<ClassEnum> flags(ClassEnum::kZero);
  flags -= ClassEnum::kZero;
  EXPECT_TRUE(flags.IsEmpty());
  flags.Set({ClassEnum::kOne, ClassEnum::kTwo, ClassEnum::kThree});
  flags -= {ClassEnum::kOne, ClassEnum::kThree};
  EXPECT_EQ(flags, ClassEnum::kTwo);
  flags -= {};
  EXPECT_EQ(flags, ClassEnum::kTwo);
}

}  // namespace
}  // namespace gb

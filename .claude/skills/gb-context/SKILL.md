---
name: gb-context
description: How Game Bits composes modules with Context, ValidatedContext, ContextContract, ContextConstraint, and ContextBuilder (gb/base/context.h, validated_context.h, context_builder.h). Use when writing a factory or Create() function that has many or optional parameters, a class that creates other classes taking a contract, declaring or reusing constraints, or assembling an application's systems at its top level, in Game Bits or any project built on it.
---

# Game Bits contexts

## Why contexts exist

Game Bits is a set of composable modules. A higher level module or an
application puts many independent classes together, and each needs its own
dependencies and options. Passing them all as factory arguments, or in large
options structs, means every layer forwards values it doesn't use itself, only
because something below it needs them.

A context avoids that. It is a type-erased, hierarchical set of values that only
the top level of the program fills in. Each class declares, in its header, the
values it reads from the context and the values it adds to it, and checks them
when it is created. Classes that know nothing of each other can each require a
`FileSystem`: the program sets one `FileSystem` in the context, and every class
that needs it finds it, with no coordination between them.

The headers are the reference for each function: `src/gb/base/context.h`,
`src/gb/base/validated_context.h`, and `src/gb/base/context_builder.h` (in a
project built on Game Bits, under `$GB_DIR`). This skill covers how they are
meant to be used.

## When to use one

- **A factory or `Create()` function with many parameters, or optional ones.**
  This is the main use. Contexts are for construction contracts, not for
  passing values on every call.
- **A class that creates other classes that take a contract.** It can pass its
  context on, so it never needs to know what those classes require.

A factory that takes a contract can fail, as validation can fail, so it returns
null (or an error) rather than a half-built object. That is the right shape for
factories anyway.

A function with a couple of required parameters doesn't need a context.

## Who uses which type

- **`Context`** is used directly only by the top level code that builds the
  program's original context, such as `main()` or an application class. Build
  it with **`ContextBuilder`** rather than a series of `Set*` calls where it
  can be done inline.
- **Factory functions take a `ContextContract`**, and convert it to a
  **`ValidatedContext`** immediately. They store the `ValidatedContext` if they
  need its lifetime guarantees (see below), and otherwise let it go once the
  object is built.
- **Internal or related functions that work with the same contract** take a
  `ValidatedContext&`, rather than a contract of their own.
- **`ContextConstraint`s** declare each value, with the `GB_CONTEXT_*` macros.

## What a contract declares

**A contract declares only what the code that uses it reads or writes itself.**
It doesn't list the constraints of the classes it creates, which have contracts
of their own and check them themselves. That is what makes contexts compose:
any module can add a constraint, and anything above it in the program can meet
it (or, for an output, anything after it can use it), without every layer in
between changing.

**Constraints can be declared anywhere.** Most are public static members of
the class that uses them. A constraint that is useful to many classes can be
declared at namespace scope and shared, as `gb_cli` does with
`kConstraintCliFileSystem` and `kConstraintCliWriter`
(`src/gb/cli/cli_path.h`, `src/gb/cli/cli_command_line.h`), which
`gb_pilot`'s `CliPilotContract` reuses.

## Names

The values in a context are effectively global to the program (within a parent
chain, see below), so a value needs a name unless its type alone identifies it
uniquely in the whole application:
- **Generic types (`int`, `bool`, `std::string`, enums used as options) are
  always named**, or two classes' unrelated options would collide. Give each
  name a `kKey<What>` constant, so callers and the class share it rather than
  repeating the string.
- **A type there is only ever one of in the program is left unnamed**, such as
  `FileSystem`, `ResourceSystem`, or `Clock`. That is what lets independent
  classes share it.

The question is whether the type is unique in the application's context, not
whether it is simple. A struct that two unrelated classes might each want their
own copy of needs a name too.

## Inputs, outputs, and scopes

A contract can declare outputs as well as inputs. Code adds values to the
context for code that runs later, which is useful for state transitions, or
when whoever made the context doesn't know everything that will be in it.

| Presence       | At construction                          | Reads | Writes | At `Complete()` / destruction   |
|----------------|------------------------------------------|-------|--------|---------------------------------|
| `kInRequired`  | Must exist, or the context is invalid    | yes   | no     | —                               |
| `kInOptional`  | May exist; default is set if missing     | yes   | no     | —                               |
| `kOutRequired` | —                                        | yes   | yes    | Must exist, or completion fails |
| `kOutOptional` | —                                        | yes   | yes    | Default is set if missing       |
| `kScoped`      | —                                        | yes   | yes    | Cleared                         |

- **Inputs are read-only through the `ValidatedContext`.** Setting or clearing
  a `kIn*` value fails. `GetPtr` still returns a non-const pointer, so calling
  methods on an input object is fine; replacing it is not.
- **Anything not declared can't be read or written at all**, even if it is in
  the context. A class can only touch what its header says it touches.
- **A value a class both reads from callers and adds itself needs two
  constraints**, such as `ResourceSystem`'s `kConstraintResourceSet`
  (`kInOptional`) and `kConstraintOutResourceSet` (`kOutOptional`).
- **Outputs are checked when the `ValidatedContext` completes**: at
  `Complete()`, when it is moved over, or when it is destroyed.
- **`kScoped` values are ones a class puts in the context for others while it
  is alive**, such as `Game::kConstraintGame` (a pointer to itself) or the
  systems block-world sets up for its states. They are cleared when the
  `ValidatedContext` completes, so nothing outlives the class that owns it.
- **Defaults are written into the context itself.** After a class with a
  `kInOptional` default has validated a context, the default is in it, visible
  to everything else using that context.
- **Wrong-type checks only exist for named values.** An unnamed value is keyed
  by type, so it is simply missing if the type differs. A named value of the
  wrong type fails validation.

### Parent contexts

A context can have a parent (`SetParent`, or `ContextBuilder().SetParent(...)`).
A lookup that misses falls back to the parent, and writes always go to the
child, so a child context holds values for one call or one part of the program
without adding them to the outer context:

```cpp
auto backend = gb::VulkanBackend::Create(
    gb::ContextBuilder()
        .SetParent(context_)
        .SetValue<std::string>(gb::VulkanBackend::kKeyAppName, "Block World")
        .SetOwned<gb::VulkanWindow>(std::move(window))
        .Build());
```

Required inputs can be met by the parent. The child's own values hide the
parent's. The parent is held by `WeakPtr`, so once it is destroyed it silently
stops supplying values.

## Failure behavior

Validation and access failures don't crash. They report an error, which is
`LOG(ERROR)` by default, and then:
- An invalid contract or `ValidatedContext` has `IsValid() == false`. Every
  read on it behaves as an empty context, and every write fails.
- A disallowed read returns null, `Type{}`, the given default, or `false`.
- A disallowed write returns `false` (or null from `Release`).
- `Complete()` returns `false` and leaves the context as it was if a
  `kOutRequired` value is missing. The destructor calls `Complete()`, so a
  missing output is reported when the `ValidatedContext` goes away.

Check `IsValid()` after converting a contract, and check the result of writes
that matter. A scripted example run fails on the logged error (see "Run an
example" in Game Bits' CLAUDE.md), but code that ignores a `false` keeps
running without the value.

## Declaring a contract

```cpp
class Foo final {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: File system Foo loads its data from. It must outlive Foo.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // OPTIONAL: Maximum number of items. Defaults to kDefaultMaxItems.
  static inline constexpr const char* kKeyMaxItems = "max_items";
  static inline constexpr int kDefaultMaxItems = 100;
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintMaxItems, kInOptional,
                                             int, kKeyMaxItems,
                                             kDefaultMaxItems);

  // Contract for creating a Foo.
  using Contract = ContextContract<kConstraintFileSystem, kConstraintMaxItems>;

  // Creates a Foo, or returns null if the contract is invalid.
  static std::unique_ptr<Foo> Create(Contract contract);
  ...
};
```

- Constraint constants are `kConstraint<What>`, in a "Contract constraints"
  section, each with a comment saying whether it is required and what it is
  for. The `static` before the macro is needed inside a class.
- Give an optional value's default in the header (`kDefault<What>`), rather
  than handling the missing case in the implementation.
- Name the contract for what it is used for: `Contract` for creation, or
  `CreateContract`, `LoadContract`, `TextureLoadContract` when a class has more
  than one.
- Extend a base class's contract with
  `DerivedContextContract<Base::Contract, kNewA, kNewB>` (as `BlockWorld` does
  with `Game::GameContract`), rather than repeating its constraints.
- A class with nothing to declare still uses `ContextContract<>` where its base
  expects a `Contract` (as `GameState` does).

## Implementing a factory

Take the contract by value, convert it, and check that it is valid before doing
anything else:

```cpp
std::unique_ptr<Foo> Foo::Create(Contract contract) {
  ValidatedContext context(std::move(contract));
  if (!context.IsValid()) {
    return nullptr;
  }
  return absl::WrapUnique(new Foo(std::move(context)));
}

Foo::Foo(ValidatedContext context) : context_(std::move(context)) {
  file_system_ = context_.GetPtr<FileSystem>();
  max_items_ = context_.GetValue<int>(kKeyMaxItems);
}
```

- **Read values once, into members.** A context lookup takes a lock and a hash
  lookup, which is too heavy for regular access. Values in a context aren't
  meant to change or be deleted while the program runs (doing so is an
  anti-pattern), so a cached copy stays right.
- **Keep the `ValidatedContext` as a member when the class holds pointers to
  objects in it**, so they live as long as the class does (when the context is
  owned, which it is when built with `ContextBuilder`). Keep it too if the class
  has `kScoped` or `kOut*` values, which it clears or checks when the class is
  destroyed, or creates other classes from it later. Otherwise, let it go.
- Use the `ValidatedContext`'s methods, not `GetContext()`'s, which bypass the
  constraints.
- **To create a class that takes a contract**, construct that contract from the
  `ValidatedContext` (`ResourceFileWriter::Create(context_)` in
  `RenderSystem`). It shares the context, and is checked against the other
  class's constraints, not this one's. Or build a child context whose parent is
  this one, when the other class needs values that only this class provides.

## Building the context at the top

```cpp
SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  gb::Context context;
  context.SetOwned(gb::CreateCliFileSystem());
  return gb::RunCommandLine(&context, &kProgram, gb::GetMainArgs(argc, argv));
}
```

```cpp
auto foo = Foo::Create(gb::ContextBuilder()
                           .SetParent(app_context)
                           .SetValue<int>(Foo::kKeyMaxItems, 50)
                           .Build());
if (foo == nullptr) { ... }
```

- **The top level knows everything the program's parts need**, and is the only
  code that has to. It puts the shared systems in one context and passes it
  (or a child of it) to each factory.
- **`SetPtr` lends an object; `SetOwned` and `SetNew` give it.** A lent object
  must outlive everything that keeps the context. Give ownership when the callee
  is the natural owner, as block-world does when it hands the `VulkanWindow`
  to the backend and the backend to the `RenderSystem`.
- **Write the type explicitly** on `SetValue<int>(...)`, `SetPtr<Base>(...)`,
  and `SetOwned<Base>(...)`. `SetValue` requires it. For the others, the value
  is stored under the type written, so `SetOwned(std::make_unique<Derived>())`
  stores a `Derived`, which a constraint on `Base` doesn't see.
- **A raw `Context*` (`Create(&context)`) is an optimization**, which avoids
  moving the context, and the contract then doesn't own it: the context must
  outlive the contract and anything that keeps it. Avoid it unless it is
  warranted. The backlog item *Contracts own their contexts* plans to remove
  it.

## Testing

- Build contexts in tests the same way callers do, with fakes for the
  interfaces (`FakeClock`, a `FileSystem` over a fake protocol, and so on).
- To check outputs after the code under test completes the context, keep a
  `std::shared_ptr<Context>`, pass it in, and read the values from it.
- To check that validation or access errors are reported, capture them with
  `ValidatedContext::SetGlobalErrorCallback` in the fixture's constructor, and
  restore it with `SetGlobalErrorCallback(nullptr)` in its destructor, as
  `src/gb/cli/run_command_line_test.cc` does. Without that, a test that
  expects an error logs it at `ERROR`.

## Examples in the tree

- `src/gb/file/local_file_protocol.h`: named options with keys and defaults.
- `src/gb/render/render_system.h` and `.cc`: required systems, a debug default
  that depends on the build, a second contract for loads, and passing the
  context on to `ResourceFileWriter` and `ResourceFileReader`.
- `src/gb/resource/resource_file_reader.h`: `kScoped` values put in the
  context during a load, for the readers it calls.
- `src/gb/cli/cli_command_line.h` and `src/gb/cli/cli_path.h`: shared
  constraints at namespace scope, and an output (`kConstraintCliUsageError`).
- `src/gb/game/game.h` and `examples/block-world/block_world.h`: a base
  contract, and a `DerivedContextContract` that adds the game's own `kScoped`
  systems for its states.
- `examples/block-world/block_world.cc` and `examples/render-check/main.cc`:
  building the application context, with `ContextBuilder` and `SetParent`.

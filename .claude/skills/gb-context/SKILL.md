---
name: gb-context
description: How to use Game Bits' Context, ValidatedContext, ContextContract, ContextConstraint, and ContextBuilder (gb/base/context.h, validated_context.h, context_builder.h) to pass dependencies and options into classes, and the conventions for declaring contracts. Use when writing or changing code that declares a contract, takes or stores a ValidatedContext, builds a Context to call a Create() function, or tests such code, in Game Bits or any project built on it.
---

# Game Bits contexts

A `Context` is a bag of values keyed by type and an optional name. Game Bits
uses it to hand a class its dependencies (systems, interfaces) and options
(flags, sizes) without long constructor parameter lists. A `ContextContract`
in the class's header declares which values it reads, writes, and requires, and
a `ValidatedContext` enforces that declaration at run time.

The headers are the reference for each function: `src/gb/base/context.h`,
`src/gb/base/validated_context.h`, and `src/gb/base/context_builder.h` (in a
project built on Game Bits, under `$GB_DIR`). This skill covers how the pieces
are meant to be used together. The comments in `validated_context.h` that
show examples (`kOptional`, `ValidatedContext<kSize>`) are out of date; follow
the examples here and in the code listed under "Examples in the tree".

## The pieces

- **`Context`**: thread-safe storage. At most one unnamed value per type, and
  at most one value per name (whatever its type). Values are owned
  (`SetNew`, `SetOwned`, `SetValue`) or borrowed (`SetPtr`). Lookups that miss
  fall back to the parent context (`SetParent`), which is held by `WeakPtr`, so
  a destroyed parent silently stops supplying values.
- **`ContextConstraint`**: one declared value: its type, optional name,
  presence, and optional default. Always declared with the `GB_CONTEXT_*`
  macros.
- **`ContextContract<kA, kB, ...>`**: a type naming a list of constraints. It
  is what public APIs take as a parameter. Constructing one validates the
  context immediately.
- **`ValidatedContext`**: the context plus its constraints, which every read and
  write is checked against. Implementations convert the contract into one, and
  usually keep it as a member.
- **`ContextBuilder`**: builds a `Context` inline at a call site.

## Presence

| Presence       | At construction                          | Reads | Writes | At `Complete()` / destruction             |
|----------------|------------------------------------------|-------|--------|-------------------------------------------|
| `kInRequired`  | Must exist, or the context is invalid    | yes   | no     | —                                         |
| `kInOptional`  | May exist; default is set if missing     | yes   | no     | —                                         |
| `kOutRequired` | —                                        | yes   | yes    | Must exist, or completion fails           |
| `kOutOptional` | —                                        | yes   | yes    | Default is set if missing                 |
| `kScoped`      | —                                        | yes   | yes    | Cleared                                   |

Things that follow from this and are easy to get wrong:
- **Inputs are read-only through the `ValidatedContext`.** Setting or clearing
  a `kIn*` value fails. `GetPtr` still returns a non-const pointer, so calling
  methods on an input object is fine; replacing it is not.
- **Anything not declared can't be read or written at all**, even if it is in
  the context. That is the point: a class can only touch what its header says
  it touches.
- **A value a class both reads from callers and publishes needs two
  constraints**, such as `ResourceSystem`'s `kConstraintResourceSet`
  (`kInOptional`) and `kConstraintOutResourceSet` (`kOutOptional`).
- **`kScoped` is for values the class puts in the context for others to use
  while it is alive**, such as `Game::kConstraintGame` (a pointer to itself)
  or the systems block-world sets up for its states. They are removed when the
  `ValidatedContext` completes, so nothing outlives the class that owns it.
- **Defaults are written into the context itself.** After a class with a
  `kInOptional` default has validated a context, the default is in it, visible
  to everything else using that context.
- **Required inputs can come from a parent context.** Presence is checked with
  the parent chain, but writes always go to the context itself.
- **Wrong-type checks only exist for named values.** An unnamed value is keyed
  by type, so it is simply missing if the type differs. A named value of the
  wrong type fails validation.

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

Because errors are logged rather than crashing, a scripted example run fails on
them (see "Run an example" in Game Bits' CLAUDE.md), but code that ignores a
`false` return keeps running with a missing value. Check `IsValid()` after
converting a contract, and check the result of writes that matter.

## Declaring a contract

Constraints and contracts are public static members of the class that consumes
them, in a "Contract constraints" section, each with a comment saying whether it
is required and what it is for:

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

Conventions:
- Constraint constants are `kConstraint<What>`. The `static` before the macro
  is needed inside a class.
- **Name values of plain types** (`int`, `bool`, `std::string`, enums used as
  options). An unnamed `int` would collide with any other unnamed `int` in the
  context. Give each name a `kKey<What>` constant, so callers and the
  implementation share it rather than repeating a string.
- **Leave values of a class's own types unnamed** (`FileSystem`, `Clock`,
  `RenderBackend`). There is normally one of each, and the type says what it is.
- Give a default in the header (`kDefault<What>`) when an optional value has
  one, rather than handling the missing case in the implementation.
- Name the contract for what it is used for: `Contract` for creation, or
  `CreateContract`, `LoadContract`, `TextureLoadContract` when a class has
  more than one.
- Extend a base class's contract with
  `DerivedContextContract<Base::Contract, kNewA, kNewB>` (as `BlockWorld` does
  with `Game::GameContract`), rather than repeating its constraints.
- A class with nothing to declare still uses `ContextContract<>` where its base
  expects a `Contract` (as `GameState` does).

## Implementing a class that takes a contract

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

- **Keep the `ValidatedContext` as a member** when the class needs anything
  from it after construction, or has `kScoped` or `kOut*` values: it keeps the
  context alive (when it owns or shares it), and clears or checks those values
  when the class is destroyed.
- **Read inputs once into members** rather than looking them up on every call.
  The lookups take a lock and do a hash lookup.
- Use the `ValidatedContext`'s methods, not `GetContext()`'s, which bypass the
  constraints.
- **To pass the context on to a class it creates**, construct that class's
  contract from the `ValidatedContext` (`ResourceFileWriter::Create(context_)`
  in `RenderSystem`). It shares the context, and is validated against the other
  class's constraints. Or build a child context whose parent is this one (see
  below), when the other class needs values this one doesn't have.

## Calling a function that takes a contract

Build the context inline with `ContextBuilder`, which is then moved into the
contract, so the contract owns it:

```cpp
auto foo = Foo::Create(gb::ContextBuilder()
                           .SetPtr<gb::FileSystem>(file_system)
                           .SetValue<int>(Foo::kKeyMaxItems, 50)
                           .Build());
if (foo == nullptr) { ... }
```

- **`SetPtr` lends an object; `SetOwned` and `SetNew` give it.** A lent object
  must outlive everything that keeps the context (the created object, usually).
  Give ownership when the callee is the natural owner, as block-world does
  when it hands the `VulkanWindow` to the backend and the backend to the
  `RenderSystem`.
- **Name the type explicitly** on `SetValue<int>(...)`, `SetPtr<Base>(...)`,
  and `SetOwned<Base>(...)`. `SetValue` requires it. For the others, the value
  is stored under the type written, so `SetOwned(std::make_unique<Derived>())`
  stores a `Derived`, which a constraint on `Base` doesn't see.
- **Share an application context with `SetParent`.** A program builds one
  `Context` (or keeps a `ValidatedContext`) holding its systems, and each
  `Create()` call gets a builder with `.SetParent(context_)` plus whatever is
  specific to that call. The child's own values hide the parent's, and nothing
  is written to the parent.
- **`Create(&context)` with a raw `Context*` doesn't own the context**, which
  must then outlive the contract and anything that keeps it. The backlog item
  *Contracts own their contexts* plans to remove this form; prefer
  `ContextBuilder` or moving a `Context` in.

## Testing

- Build contexts in tests the same way callers do, with fakes for the
  interfaces (`FakeClock`, a `FileSystem` over a fake protocol, and so on).
- To check outputs after the code under test completes the context, keep a
  `std::shared_ptr<Context>` and pass it in, then read the values from it.
- To check that validation or access errors are reported, capture them with
  `ValidatedContext::SetGlobalErrorCallback` in the fixture's constructor, and
  restore it with `SetGlobalErrorCallback(nullptr)` in its destructor, as
  `src/gb/cli/run_command_line_test.cc` does. Without that, a test that
  expects an error logs it at `ERROR`.

## Examples in the tree

- `src/gb/file/local_file_protocol.h`: named options with keys and defaults.
- `src/gb/render/render_system.h` and `.cc`: required systems, a debug default
  that depends on the build, a second contract for loads, passing the context
  on to `ResourceFileWriter` and `ResourceFileReader`.
- `src/gb/resource/resource_file_reader.h`: `kScoped` values put in the
  context during a load for the readers it calls.
- `src/gb/game/game.h` and `examples/block-world/block_world.h`: a base
  contract and a `DerivedContextContract` that adds the game's own `kScoped`
  systems.
- `examples/block-world/block_world.cc` and `examples/render-check/main.cc`:
  application setup with `ContextBuilder` and `SetParent`.

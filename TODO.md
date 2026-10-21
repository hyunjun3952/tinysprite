# TODO

## Follow-ups to the C++ migration
- The migration itself is done (whole project is C++17, one clang-tidy config, class / RAII document model).
  What is left is optional cleanup:
  - `History` still keeps cel pointers inside its steps (the cel must outlive them, `forgetCel` drops them); a
    shared handle (`std::shared_ptr<Cel>`) would remove that rule but means `Layer` hands out shared cels.
  - `Editor::activeCel()` / `editableCel()` return raw pointers because "no cel" is a normal answer; `std::optional<
    std::reference_wrapper<Cel>>` would be heavier than it is worth.

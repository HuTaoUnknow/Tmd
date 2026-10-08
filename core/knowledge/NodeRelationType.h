#pragma once
// Parent means a comparable alternative; Child means an explanation or example.
// These roles are independent of filesystem folders. DetailOf is the backlink
// of Child; comparable alternatives share their predecessor/successor routes.
enum class NodeRelationType { Previous, Next, Parent, Child, DetailOf };

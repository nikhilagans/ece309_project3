Project 3 Design Log

Author: Nikhil Agans
P2 code used: Instructor reference
Tools used: VS Code, WSL Ubuntu, CMake, and CTest for development, compilation, and testing.
AI assistants used and purpose: I used ChatGPT to check certain lines of code after I wrote them. 

1. Hash Table

For the ToolRegistry, I used separate chaining to handle collisions. Each bucket contains a vector of unique_ptr<Tool> objects. I chose this approach over open addressing because multiple tools can share a bucket without requiring additional probing logic. It also makes resizing easier while preserving the original Tool objects.

My implementation uses the FNV-1a hash function, which processes each character through XOR and multiplication. The resulting hash determines which bucket stores a tool. The registry starts with 16 buckets and uses a maximum load factor of 0.75. When another insertion would exceed this threshold, the number of buckets doubles, and the existing entries are redistributed.

I tested the registry by inserting and retrieving 10,000 uniquely named tools. I also compared FNV-1a with a weaker hash function that adds the character values of each name.

Measurement

Result

Bucket count after 10,000 tools

16,384

Load factor after 10,000 tools

0.6104

Maximum probe length after 10,000 tools

6

FNV-1a longest chain (1,000 names, 128 buckets)

12

Alternative hash longest chain (1,000 names, 128 buckets)

70

The results show that the registry maintained a load factor below 0.75 while storing 10,000 tools. In the separate comparison using 1,000 names and 128 buckets, FNV-1a produced a longest chain of 12 compared with 70 for the alternative hash. This suggests that FNV-1a distributed the tested names more evenly.

The add() operation has an amortized time complexity of O(1), assuming a well-distributed hash, because most insertions only require hashing and inserting into a bucket. Although resizing takes O(n) time, doubling the bucket count spreads this cost across many insertions.
2. Pointer Stability

An important requirement was ensuring that pointers returned by find() remain valid after resizing.

My implementation uses unique_ptr to manage dynamically allocated Tool objects. During resizing, the unique_ptr objects move into new buckets, but the actual Tool objects remain at their original memory addresses. Therefore, the raw pointers returned by find() remain valid while the tools stay registered and the registry exists.

I tested this by saving a tool's address, inserting another 200 tools to trigger resizing, and confirming that find() still returned the original address. This matters because other components may retain pointers to registered tools.

3. Calculator Trace

For the calculator, I used two stacks: one for numerical values and another for operators. The calculator uses operator precedence and parentheses to determine which operations should be performed first.

For the expression `2 * (3 + 4) - 5`, the stacks change as shown below. The rightmost value in each stack represents the top.

| Token | Values Stack | Operators Stack |
|-------|--------------|-----------------|
| `2`   | `[2]`        | `[]`            |
| `*`   | `[2]`        | `[*]`           |
| `(`   | `[2]`        | `[*, (]`        |
| `3`   | `[2, 3]`     | `[*, (]`        |
| `+`   | `[2, 3]`     | `[*, (, +]`     |
| `4`   | `[2, 3, 4]`  | `[*, (, +]`     |
| `)`   | `[2, 7]`     | `[*]`           |
| `-`   | `[14]`       | `[-]`           |
| `5`   | `[14, 5]`    | `[-]`           |
| End   | `[9]`        | `[]`            |

The calculator first pushes 2 onto the values stack and then pushes the multiplication operator onto the operators stack. The opening parenthesis is added to the operators stack, followed by 3, the addition operator, and 4.

When the closing parenthesis is reached, the calculator performs `3 + 4`, leaving 7 on the values stack. The opening parenthesis is then removed.

When the subtraction operator is encountered, multiplication is performed first because it has higher precedence. This produces `2 * 7 = 14`. The subtraction operator and 5 are then added to their respective stacks.

Finally, the calculator evaluates `14 - 5`, producing the final result of **9**.

4. Errors as Values

I used return values and error strings to communicate expected failures instead of throwing exceptions. For example, Workspace::resolve() returns false when a path attempts to escape the workspace, while calc_evaluate() returns false for division by zero.

These situations can occur during normal operation because users may provide invalid inputs. Returning an error allows the calling function to respond appropriately without interrupting the entire program. It also makes failure conditions explicit.

Exceptions are more appropriate for unexpected problems, such as memory allocation failures, where normal processing may not be able to continue safely.

5. What I Would Design Differently

If I redesigned the project, I would make the hash table's initial bucket count and load-factor threshold configurable. This would allow me to compare collision frequency and memory usage across different configurations.

I would also expand the calculator tests to cover more complicated expressions and invalid inputs. My final implementation passed all 20 Project 3 unit tests and all four CTest suites, including end-to-end testing. These results helped verify that the components worked individually and together.

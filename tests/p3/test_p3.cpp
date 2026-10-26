#include "../mini_test.h"
#include "../test_helpers.h"

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Simple tool used to test the registry.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n)
        : n_(std::move(n)), schema_(JsonValue::object()) {}

    std::string name() const override {
        return n_;
    }

    std::string description() const override {
        return "test tool";
    }

    const JsonValue& schema() const override {
        return schema_;
    }

    ToolResult invoke(const JsonValue&, ToolContext&) override {
        return ToolResult::success(n_);
    }

private:
    std::string n_;
    JsonValue schema_;
};

// Tokenize an expression and evaluate it.
bool eval(const std::string& expr,
          double* value,
          std::string* error) {
    std::vector<CalcToken> tokens;

    if (!calc_tokenize(expr, &tokens, error)) {
        return false;
    }

    return calc_evaluate(tokens, value, error);
}

} // namespace

// ============================================================
// ORIGINAL REGISTRY TESTS
// ============================================================

TEST(reg_add_and_find) {
    ToolRegistry r;

    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();

    REQUIRE(r.add(std::move(t)));

    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;

    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();

    REQUIRE(r.add(std::move(first)));

    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

// ============================================================
// ADDITIONAL REGISTRY TESTS
// ============================================================

TEST(reg_null_rejected) {
    ToolRegistry r;

    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
}

TEST(reg_empty_registry) {
    ToolRegistry r;

    CHECK(r.size() == 0);
    CHECK(r.find("missing") == nullptr);
    CHECK(r.bucket_count() > 0);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;

    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));

    std::vector<std::string> names;

    r.for_each([&](const Tool& tool) {
        names.push_back(tool.name());
    });

    CHECK(names.size() == 3);

    int alpha = 0;
    int beta = 0;
    int gamma = 0;

    for (const auto& name : names) {
        if (name == "alpha") ++alpha;
        if (name == "beta") ++beta;
        if (name == "gamma") ++gamma;
    }

    CHECK(alpha == 1);
    CHECK(beta == 1);
    CHECK(gamma == 1);
}

TEST(reg_rehash_preserves_tools) {
    ToolRegistry r;

    std::size_t original_buckets = r.bucket_count();

    for (int i = 0; i < 100; ++i) {
        REQUIRE(r.add(
            std::make_unique<NamedTool>(
                "tool_" + std::to_string(i)
            )
        ));
    }

    CHECK(r.bucket_count() > original_buckets);
    CHECK(r.size() == 100);

    for (int i = 0; i < 100; ++i) {
        CHECK(
            r.find("tool_" + std::to_string(i)) != nullptr
        );
    }
}

TEST(reg_pointer_stability) {
    ToolRegistry r;

    auto first = std::make_unique<NamedTool>("original");
    Tool* original_address = first.get();

    REQUIRE(r.add(std::move(first)));

    for (int i = 0; i < 200; ++i) {
        REQUIRE(r.add(
            std::make_unique<NamedTool>(
                "tool_" + std::to_string(i)
            )
        ));
    }

    CHECK(r.find("original") == original_address);
}

TEST(reg_collision_handling) {
    ToolRegistry r;

    std::size_t buckets = r.bucket_count();

    std::string first = "collision_0";
    std::string second;

    for (int i = 1; i < 1000; ++i) {
        std::string candidate =
            "collision_" + std::to_string(i);

        if (ToolRegistry::hash(first) % buckets ==
            ToolRegistry::hash(candidate) % buckets) {
            second = candidate;
            break;
        }
    }

    REQUIRE(!second.empty());

    REQUIRE(r.add(std::make_unique<NamedTool>(first)));
    REQUIRE(r.add(std::make_unique<NamedTool>(second)));

    CHECK(r.find(first) != nullptr);
    CHECK(r.find(second) != nullptr);
    CHECK(r.max_probe_length() >= 2);
}

TEST(reg_10000_tools) {
    ToolRegistry r;

    for (int i = 0; i < 10000; ++i) {
        REQUIRE(r.add(
            std::make_unique<NamedTool>(
                "tool_" + std::to_string(i)
            )
        ));
    }

    CHECK(r.size() == 10000);

    for (int i = 0; i < 10000; ++i) {
        CHECK(
            r.find("tool_" + std::to_string(i)) != nullptr
        );
    }

    CHECK(r.load_factor() <= 0.75);

    std::printf(
        "Registry statistics: buckets=%zu, load=%.4f, max_probe=%zu\n",
        r.bucket_count(),
        r.load_factor(),
        r.max_probe_length()
    );

    CHECK(r.max_probe_length() < 20);
}

TEST(reg_alternative_hash) {
    // A weak hash that adds ASCII character values.
    auto weak_hash = [](const std::string& name) {
        std::size_t result = 0;

        for (unsigned char c : name) {
            result += c;
        }

        return result;
    };

    const std::size_t buckets = 128;

    std::vector<int> weak_counts(buckets, 0);
    std::vector<int> strong_counts(buckets, 0);

    for (int i = 0; i < 1000; ++i) {
        std::string name =
            "tool_" + std::to_string(i);

        ++weak_counts[weak_hash(name) % buckets];

        ++strong_counts[
            ToolRegistry::hash(name) % buckets
        ];
    }

    int weak_longest = 0;
    int strong_longest = 0;

    for (std::size_t i = 0; i < buckets; ++i) {
        if (weak_counts[i] > weak_longest) {
            weak_longest = weak_counts[i];
        }

        if (strong_counts[i] > strong_longest) {
            strong_longest = strong_counts[i];
        }
    }

    std::printf(
        "Hash comparison: weak longest=%d, FNV longest=%d\n",
        weak_longest,
        strong_longest
    );

    CHECK(strong_longest < weak_longest);
}

// ============================================================
// ORIGINAL WORKSPACE TEST
// ============================================================

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());

    fs::path out;
    std::string reason;

    REQUIRE(
        ws.resolve("notes/todo.txt", &out, &reason)
    );

    CHECK(
        out == ws.root() / "notes" / "todo.txt"
    );

    CHECK(
        !ws.resolve("../etc/passwd", &out, &reason)
    );

    CHECK(!reason.empty());
}

// ============================================================
// ADDITIONAL WORKSPACE TESTS
// ============================================================

TEST(ws_rejects_absolute_path) {
    TempDir d;
    Workspace ws(d.path());

    fs::path out;
    std::string reason;

    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_empty_path) {
    TempDir d;
    Workspace ws(d.path());

    fs::path out;
    std::string reason;

    CHECK(!ws.resolve("", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_accepts_normalized_path) {
    TempDir d;
    Workspace ws(d.path());

    fs::path out;
    std::string reason;

    REQUIRE(
        ws.resolve("notes/../README.txt", &out, &reason)
    );

    CHECK(
        out == ws.root() / "README.txt"
    );

    CHECK(reason.empty());
}

TEST(ws_rejects_null_character) {
    TempDir d;
    Workspace ws(d.path());

    fs::path out;
    std::string reason;

    std::string path = "notes/file";
    path.push_back('\0');
    path += ".txt";

    CHECK(!ws.resolve(path, &out, &reason));
    CHECK(!reason.empty());
}

// ============================================================
// ORIGINAL CALCULATOR TESTS
// ============================================================

TEST(calc_precedence) {
    double v = 0;
    std::string err;

    REQUIRE(eval("2 + 3 * 4", &v, &err));

    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;

    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ============================================================
// ADDITIONAL CALCULATOR TESTS
// ============================================================

TEST(calc_left_to_right) {
    double value = 0;
    std::string error;

    REQUIRE(eval("20 - 5 - 3", &value, &error));

    CHECK(value == 12);
}

TEST(calc_parentheses) {
    double value = 0;
    std::string error;

    REQUIRE(eval("(2 + 3) * 4", &value, &error));

    CHECK(value == 20);
}

TEST(calc_negative_numbers) {
    double value = 0;
    std::string error;

    REQUIRE(eval("-5 + 3", &value, &error));

    CHECK(value == -2);
}

// ============================================================
// MAIN TEST RUNNER
// ============================================================

int main(int argc, char** argv) {
    return mt::run_all(argc, argv);
}
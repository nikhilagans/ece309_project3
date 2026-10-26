#include "tools/calculator.h"

#include <stack>
#include <string>
#include <vector>

// Determine an operator's priority.
static int precedence(CalcToken::Kind op) {
    if (op == CalcToken::Kind::Times ||
        op == CalcToken::Kind::Divide) {
        return 2;
    }

    if (op == CalcToken::Kind::Plus ||
        op == CalcToken::Kind::Minus) {
        return 1;
    }

    return 0;
}

// Apply the top operator to the top two values.
static bool apply_operator(
    std::stack<double>& values,
    std::stack<CalcToken::Kind>& operators,
    std::string* error) {

    if (values.size() < 2 || operators.empty()) {
        *error = "invalid expression";
        return false;
    }

    double right = values.top();
    values.pop();

    double left = values.top();
    values.pop();

    CalcToken::Kind op = operators.top();
    operators.pop();

    double result = 0.0;

    switch (op) {
        case CalcToken::Kind::Plus:
            result = left + right;
            break;

        case CalcToken::Kind::Minus:
            result = left - right;
            break;

        case CalcToken::Kind::Times:
            result = left * right;
            break;

        case CalcToken::Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;

        default:
            *error = "invalid operator";
            return false;
    }

    values.push(result);
    return true;
}

// Evaluate the token list using two stacks.
bool calc_evaluate(const std::vector<CalcToken>& tokens,
                   double* out,
                   std::string* error) {

    std::stack<double> values;
    std::stack<CalcToken::Kind> operators;

    for (const CalcToken& token : tokens) {
        CalcToken::Kind kind = token.kind;

        if (kind == CalcToken::Kind::Number) {
            values.push(token.value);
        }

        else if (kind == CalcToken::Kind::LParen) {
            operators.push(kind);
        }

        else if (kind == CalcToken::Kind::RParen) {
            while (!operators.empty() &&
                   operators.top() != CalcToken::Kind::LParen) {

                if (!apply_operator(values, operators, error)) {
                    return false;
                }
            }

            if (operators.empty()) {
                *error = "mismatched parentheses";
                return false;
            }

            operators.pop();
        }

        else {
            while (!operators.empty() &&
                   operators.top() != CalcToken::Kind::LParen &&
                   precedence(operators.top()) >= precedence(kind)) {

                if (!apply_operator(values, operators, error)) {
                    return false;
                }
            }

            operators.push(kind);
        }
    }

    // Apply remaining operators.
    while (!operators.empty()) {
        if (operators.top() == CalcToken::Kind::LParen) {
            *error = "mismatched parentheses";
            return false;
        }

        if (!apply_operator(values, operators, error)) {
            return false;
        }
    }

    if (values.size() != 1) {
        *error = "invalid expression";
        return false;
    }

    *out = values.top();
    error->clear();
    return true;
}
#ifndef CONTRACTS_H
#define CONTRACTS_H

#include <configuration/hardening.h>
#include <cstdio>
#include <cstdlib>
#include <exception>

// A contract is a formal interface specification for a software
// component such as a class or member-function. It is a set of
// conditions that express expectations on how the component
// interoperates with other components of the program.
//
// A contract violation occurs when a condition that is a part of
// the contract does not hold.
//
// A pre-condition is part of the function contract where the
// responsibility of satisfying it rests with the caller. Generally,
// these requirements are placed on the arguments passed to the
// function upon entry into the function.
//
// A postcondition is a part of the function contract where the
// responsibiluty of satisfying the condition rests with the callee;
// the implementer of the function itself. These are generally
// conditions that will hold true regarding the return value of the
// function when it completes execution normally.
//
// An invariant is a condition on the state ofan object, or a set of
// obsets, that is maintained over a certain amount of time.
// A class invariant is a condition that a class type maintains
// throughout the lifetime of an object. There are other kinds of
// invariants such as loop invariants. Often these are expected to hold
// on the entry or exit of functions

// libsoa provides several hardening modes, where each mode enables a
// set of assertions that prevent UB caused by contract violations.
// of the library. Different hardening modes make different tradeoffs
// between checking and runtime performance.

// If the user does not configure the hardening level, default to
// HARDENING_MODE_NONE
#ifndef HARDENING_MODE
	#define HARDENING_MODE HARDENING_MODE_NONE
#endif

// If the hardening level chosen by the user is invalid, error out
#if HARDENING_MODE != HARDENING_MODE_NONE && HARDENING_MODE != HARDENING_MODE_FAST &&                                  \
    HARDENING_MODE != HARDENING_MODE_DEBUG
	#error HARDENING_MODE must be set to one of the following values: \
HARDENING_MODE_NONE, \
HARDENING_MODE_FAST, \
HARDENING_MODE_DEBUG
#endif

// contract assert behavior (default: IO-logging and trap)
#ifndef ASSERT_MODE_NONE
	#define ASSERT_MODE_NONE 0
	#define ASSERT_MODE_TRAP 1
	#define ASSERT_MODE_LOG_TRAP 2
	#define ASSERT_MODE_ABORT 3
	#define ASSERT_MODE_LOG_ABORT 4
#endif

#ifndef ASSERT_MODE
	#define ASSERT_MODE ASSERT_MODE_LOG_TRAP // default assertion mode
#else
	#if (ASSERT_MODE > ASSERT_MODE_LOG_ABORT)
		#undef ASSERT_MODE
		#define ASSERT_MODE ASSERT_MODE_NONE
	#endif
#endif

// Definition of TRAP macro
// __builtin_trap is a GCC + clang builtin
#ifndef TRAP
	#if __has_builtin(__builtin_trap) || defined(__GNUC__)
		#define TRAP() __builtin_trap()
	#elif defined(_MSC_VER)
		// __debugbreak() alone doesn't mark the site as noreturn to
		// the optimizer;
		// __assume(false) tells MSVC that the control never returns
		#define TRAP() (__debugbreak(), __assume(false))
	#else
		#define TRAP() std::abort()
	#endif
#endif

namespace soa {

	struct AssertViolation {
		const char *expression_; // stringified condition
		const char *comment_;    // user message
		const char *file_name_;
		int line_number_;
		const char *category_;

		[[nodiscard]] const char *expression() const { return expression_; }
		[[nodiscard]] const char *file_name() const { return file_name_; }

		[[nodiscard]] const char *comment() const { return comment_; }

		[[nodiscard]] int line_number() const { return line_number_; }

		[[nodiscard]] const char *category() const { return category_; }
	};

	using ViolationHandler = void (*)(const AssertViolation &);

} // namespace soa

// Definition of TRAP_WITH_MSG macro
// __builtin_verbose_trap is clang only
#ifndef TRAP_WITH_MSG
	#if defined(__clang__) && defined(__has_builtin)
		#if __has_builtin(__builtin_verbose_trap)
			#if defined(__apple_build_version__) && __apple_build_version__ < 17000000
				#define TRAP_WITH_MSG(tag, msg) __builtin_verbose_trap(tag ", " msg)
			#else
				#define TRAP_WITH_MSG(tag, msg) __builtin_verbose_trap(tag, msg)
			#endif
		#endif
	#else
		#define TRAP_WITH_MSG(tag, msg) TRAP()
	#endif
#endif

#ifndef STRINGIFY
	#define STRINGIFY_IMPL(x) #x
	#define STRINGIFY(x) STRINGIFY_IMPL(x)
#endif

#define VIOLATION(cond, category, msg) ::soa::AssertViolation(#cond, msg, __FILE__, __LINE__, category)

namespace soa {
	struct Assert {
		static void contract_fail_handler_default(const soa::AssertViolation &violation) {
			std::fprintf(stderr,
			             "%s:%d: contract violation: %s\n",
			             violation.file_name(),
			             violation.line_number(),
			             violation.comment());
			std::fflush(stderr);
		}

		static void set_contract_fail_handler(ViolationHandler handler) { violationHandler = handler; }

		static void contract_fail_handler(const AssertViolation &violation) { (*violationHandler)(violation); }

	private:
		static inline ViolationHandler violationHandler{&contract_fail_handler_default};
	};
} // namespace soa

#if ASSERT_MODE == ASSERT_MODE_NONE
	#define CONTRACT_ASSERT(cond, category, msg) ((void)0)
#elif ASSERT_MODE == ASSERT_MODE_TRAP
	#define ON_FAILURE(cond, category, msg) TRAP()
#elif ASSERT_MODE == ASSERT_MODE_LOG_TRAP
  // TODO : Implement LOG_TRAP case
	#define ON_FAILURE(cond, category, msg)                                                                            \
		(::soa::Assert::contract_fail_handler(VIOLATION(cond, category, msg)),                                         \
		 TRAP_WITH_MSG(#cond, __FILE__ ":" STRINGIFY(__LINE__) ": " msg category))
#elif ASSERT_MODE == ASSERT_MODE_ABORT
	#define ON_FAILURE(cond, category, msg)                                                                            \
		(::soa::Assert::contract_fail_handler(VIOLATION(cond, category, msg)), std::abort())
#elif ASSERT_MODE == ASSERT_MODE_LOG_ABORT
	#define ON_FAILURE(cond, category, msg) std::abort()
#endif

#if ASSERT_MODE == ASSERT_MODE_NONE
	#define CONTRACT_ASSERT(cond, category, msg) ((void)0)
#else
	#define CONTRACT_ASSERT(cond, category, msg) ((cond) ? (void)0 : ON_FAILURE(cond, category, msg))
#endif

// Individual assertions are grouped into assertion categories. Each hardening mode enables a
// different set of assertion categories. Categories provide a layer of abstraction that make it easier
// to reason about a class or function's correctness.

// valid-input-range.
// Check that the range (expressed as a pair or iterators, an iterator and a sentinel or iterator +
// size) given as input to a function is valid:
// - The sentinel is reachable from the begin iterator.
// Violating assertions in this category lead to out-of-bounds access.

// non-null.
// Checks that the pointer being dereferenced is not nullptr.
// Violating assertions in this category can still cause UB.

// non-overlapping ranges.
// For functions that accept multiple input ranges, check that they are non-overlapping.

// valid-deallocation
// checks that an attempt to deallocate memory is valid. (e.g. the given object was allocated
// by the given allocator). Violating this category typically results in a memory leak.

// valid-external-api-call.
// Checs that a call to an external API doesn't fail in an unexpected manner.

// arguments-within-domain.
// Checks that the given argument is within the domain of the valid arguments for the function. Any API
// external to the library falls in this category.

// pedantic.
// Checks pre-conditions imposed by the C++ standard.

// uncategorized.
// For all assertions that haven't been categorized yet.
#if HARDENING_MODE == HARDENING_MODE_NONE

	#define ASSERT_VALID_INPUT_RANGE(expression, message) ((void)0)
	#define ASSERT_VALID_ELEMENT_ACCESS(expression, message) ((void)0)
	#define ASSERT_NON_NULL(expression, message) ((void)0)
	#define ASSERT_NON_OVERLAPPING_RANGES(expression, message) ((void)0)
	#define ASSERT_VALID_DEALLOCATION(expression, message) ((void)0)
	#define ASSERT_VALID_EXTERNAL_API_CALL(expression, message) ((void)0)
	#define ASSERT_COMPATIBLE_ALLOCATOR(expression, message) ((void)0)
	#define ASSERT_ARGUMENT_WITHIN_DOMAIN(expression, message) ((void)0)
	#define ASSERT_PEDANTIC(expression, message) ((void)0)
	#define ASSERT_UNCATEGORIZED(expression, message) ((void)0)

#elif HARDENING_MODE == HARDENING_MODE_FAST

	// Enabled checks.
	#define ASSERT_VALID_INPUT_RANGE(expression, message) CONTRACT_ASSERT(expression, "valid-input-range", message)
	#define ASSERT_VALID_ELEMENT_ACCESS(expression, message)                                                           \
		CONTRACT_ASSERT(expression, "valid-element-access", message)
  // Disabled checks.
	// On most modern platforms, dereferencing a null pointer does not
	// lead to an actual memory access.
	#define ASSERT_NON_NULL(expression, message) ((void)0)
	#define ASSERT_NON_OVERLAPPING_RANGES(expression, message) ((void)0)
	#define ASSERT_VALID_DEALLOCATION(expression, message) ((void)0)
	#define ASSERT_VALID_EXTERNAL_API_CALL(expression, message) ((void)0)
	#define ASSERT_COMPATIBLE_ALLOCATOR(expression, message) ((void)0)
	#define ASSERT_ARGUMENT_WITHIN_DOMAIN(expression, message) ((void)0)
	#define ASSERT_PEDANTIC(expression, message) ((void)0)
	#define ASSERT_UNCATEGORIZED(expression, message) ((void)0)

#elif HARDENING_MODE == HARDENING_MODE_DEBUG
  // Enabled checks.
	#define ASSERT_VALID_INPUT_RANGE(expression, message) CONTRACT_ASSERT(expression, "valid-input-range", message)
	#define ASSERT_VALID_ELEMENT_ACCESS(expression, message)                                                           \
		CONTRACT_ASSERT(expression, "valid-element-access", message)
	#define ASSERT_NON_NULL(expression, message) CONTRACT_ASSERT(expression, "non-null", message)
	#define ASSERT_NON_OVERLAPPING_RANGES(expression, message)                                                         \
		CONTRACT_ASSERT(expression, "non-overlapping-ranges", message)
	#define ASSERT_VALID_DEALLOCATION(expression, message) CONTRACT_ASSERT(expression, "valid-deallocation", message)
	#define ASSERT_VALID_EXTERNAL_API_CALL(expression, message)                                                        \
		CONTRACT_ASSERT(expression, "valid-external-api-call", message)
	#define ASSERT_COMPATIBLE_ALLOCATOR(expression, message)                                                           \
		CONTRACT_ASSERT(expression, "compatible-allocator", message)
	#define ASSERT_ARGUMENT_WITHIN_DOMAIN(expression, message)                                                         \
		CONTRACT_ASSERT(expression, "argument-within-domain", message)
	#define ASSERT_PEDANTIC(expression, message) CONTRACT_ASSERT(expression, "pedantic", message)
	#define ASSERT_UNCATEGORIZED(expression, message) CONTRACT_ASSERT(expression, "uncategorized", message)
#endif

// Concat macro.
// ## is the token-pasting operator. It joins the tokens on either side
// into a single new token.
#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

namespace soa {
	// This scope guard is an RAII wrapper.
	template <typename Func> struct PostConditionScopeGuard {
		Func func_;
		int exceptions_;

		explicit PostConditionScopeGuard(const Func &func) : func_(func), exceptions_{std::uncaught_exceptions()} {}

		~PostConditionScopeGuard() {
			if (std::uncaught_exceptions() == exceptions_) {
				func_();
			}
		}
	};
} // namespace soa

#if HARDENING_MODE == HARDENING_MODE_DEBUG
	#define RECORD_OLD_VALUE(var_name, expression) const auto var_name = (expression)
	#define CONTRACT_POSTCONDITION(expression, msg)                                                                    \
		::soa::PostConditionScopeGuard CONCAT(_postcondition_, __COUNTER__){                                           \
		    [&] { CONTRACT_ASSERT(expression, "post-condition", msg); }};
#else
	#define RECORD_OLD_VALUE(expression, var_name) ((void)0)
	#define CONTRACT_POSTCONDITION(expression, msg) ((void)0)
#endif

#endif
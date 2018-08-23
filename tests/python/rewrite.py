# -*- mode: python; coding: utf-8 -*-
# Copyright (C) 2018 Laboratoire de Recherche et Développement de
# l'Epita (LRDE).
#
# This file is part of Spot, a model checking library.
#
# Spot is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Spot is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os, spot

def breakpt(cond=None):
    "Displays the PID of the current process and wait until enter is pressed."
    if cond is False:
        return

    print('PID:', os.getpid())
    input()

# Helpers
#
def parse_recipe(recipe):
    if isinstance(recipe, spot.rewriting_recipe):
        return recipe

    if not isinstance(recipe, str):
        recipe = spot.parse_rewriting_recipe(*recipe)
    else:
        recipe = spot.parse_rewriting_recipe(recipe)

    assert(recipe is not None)

    return recipe

def rewrite(f, recipe):
    recipe = parse_recipe(recipe)

    if isinstance(f, str):
        f = spot.formula(f)

    assert(f is not None)

    f = recipe.rewrite(f)

    assert(f is not None)

    return f


CR = '\033[91m'
CG = '\033[92m'
CB = '\033[94m'
CEND = '\033[0m'
LPAD = 25

if os.name == 'nt':
    CRED, CR, CG, CB = '', '', '', ''

invalid_tests = [
    # Unmatched parenthesis
    '( Ff = 1', ' Gg ) = 1',

    # Unknown identifier
    # Note: we no longer test for this, since more complex scenarios can now
    #       be added with user-defined patterns and outputs
    # 'Ff = Gg'
]

for test in invalid_tests:
    try:
        spot.parse_rewriting_recipe(test)
        assert False, '{} is not invalid'.format(test)
    except SyntaxError:
        pass

# Having multiple patterns and outputs at the same time
r = spot.parse_rewriting_recipe('XFGf = FGf / FGFf = GFf')

assert(rewrite('FGFa', r) == spot.formula('GFa'))
assert(rewrite('XGFa', r) == spot.formula('XGFa'))

r.extend('XGFf = GFf / GFGf = FGf')

assert(rewrite('XGFa', r) == spot.formula('GFa'))
assert(rewrite('XFGXGFa', r) == spot.formula('GFa'))


# Replacing patterns
not_f = spot.parse_rewriting_pattern('!f')
p = spot.parse_rewriting_pattern('GFf', { 'f': not_f })

assert(str(p).strip() == 'GF!f')

r = spot.parse_rewriting_recipe('GFf = FGf', { 'f': not_f })

assert(str(r).strip() == 'GF!f ≡ FGf')


# Creating recipes manually
pat = spot.parse_rewriting_pattern('XFGf')
out = spot.parse_rewriting_output('FGf')
r   = spot.rewriting_recipe(pat, out)

assert(r.rewrite(spot.formula('XFGa')) == spot.formula('FGa'))
assert(r.rewrite(spot.formula('XGFa')) == spot.formula('XGFa'))

r.extend('XGFf = GFf')

assert(r.rewrite(spot.formula('XFGa')) == spot.formula('FGa'))
assert(r.rewrite(spot.formula('XGFa')) == spot.formula('GFa'))

# Adding conditions
# Here, the recipe is only applied if the atomic proposition picked up by 'f'
# has the name 'a'.
r = spot.parse_rewriting_recipe('XFGf = FGf',
                                predicate={ 'f': lambda f: f.ap_name() == 'a' })

assert(r.rewrite(spot.formula('XFGa')) == spot.formula('FGa'))
assert(r.rewrite(spot.formula('XFGb')) == spot.formula('XFGb'))


# Custom patterns and outputs

class NoConstantsPattern(spot.python_rewriting_pattern):
    """
    Rewriting pattern that matches all formulas EXCEPT constants
    (ff, tt, eword), and saves them as '*'.
    """
    output = None

    def __init__(self):
        spot.python_rewriting_pattern.__init__(self)
    
    def match(self, match, f):
        if f.kind() in (spot.op_ff, spot.op_tt, spot.op_eword):
            return spot.rewriting_state.nomatch()

        breakpt()

        # FIXME: Swig somehow messes up the 'output' pointer, which causes a
        # segfault
        o = match.with_formula('*', f).with_output(self.output)
        print(o)
        return o

    def try_merge(self, _):
        return False
    
    def associate(self, output):
        self.output = output
    
    def to_string(self, _, __):
        return '*'

class NegatingOutput(spot.python_rewriting_output):
    """
    Rewriting output that returns the negation of the formula named '*'.
    """

    def __init__(self):
        spot.python_rewriting_output.__init__(self)
    
    def create(self, match):
        formulas = state.formulas('*')

        for i, f in enumerate(formulas):
            formulas[i] = spot.formula.Not(f)

        return formulas
    
    def to_string(self, _):
        return '!*'

no_constants_pattern = NoConstantsPattern()
negating_output = NegatingOutput()

r = spot.parse_rewriting_recipe('GFf = FGf', { 'f': no_constants_pattern },
                                             { 'f': negating_output })

assert(rewrite('GF1', r) == spot.formula('GF1'))
# assert(rewrite('GFa', r) == spot.formula('GF!a'))


# Rewriting tests

rewriting_tests = [
    # Simple substitution
    ('XFGf = FGf', 'XFGa', 'FGa'),

    # More complicated substitution
    ('XFGf = FGf', 'XFG(f & g)', 'FG(f & g)'),

    # Multiple binary substitutions
    ('f & (g U h) = g & (f U h)',
     'a & b & (c U d) & (e U f)',
     'c & e & (a U d) & (b U f)'),
    
    # Recursive substitution
    ('XFGf = FGf', 'XFG(XFGf U XFG(fa U fb))', 'FG(FGf U FG(fa U fb))'),
    ('XFGf = FGf', 'XFG(XFGf & XFG(fa & fb))', 'FG(FGf & FG(fa & fb))'),

    # Conditions on eventuality / universality
    ('f & g = 1', 'a & b', '1'),
    ('f & e = 1', 'a & b', 'a & b'),
    ('f & u = 1', 'a & b', 'a & b'),

    # Equality
    ('f U g U f = 1', 'a U b U a', '1'),
    ('f U g U f = 1', 'a U b U c', 'a U (b U c)'),

    # Picking up values
    ('f[*1..j] = f[*0..j]', '{ a[*1..5] }', '{ a[*0..5] }'),
    ('f[*1..j] = f[*0..j]', '{ a[*2..5] }', '{ a[*2..5] }')
]

for recipe, given, expected in rewriting_tests:
    print('Rewriting ', CB, given.rjust(LPAD), CEND, ' into ', CB, expected, CEND, sep='')
    rewritten = rewrite(given, recipe)

    if rewritten != expected:
        print()
        print('Formula \'{}{}{}\' wrongly rewritten'.format(CR, given, CEND))
        print('-> Expected \'{}{}{}\' but got \'{}{}{}\''.format(CR, expected, CEND, CR, rewritten, CEND))
        print()
        print('Recipe:\n')
        print(CR, parse_recipe(recipe), CEND, sep='')

        exit(1)

print()

# The following section implements and tests simplification rules defined
# in lt.pdf. Before we can apply all of them though, some very specific patterns
# and outputs must be defined.

class FoldingOutput(spot.python_rewriting_output):
    "Rewriting output that repeats an output a certain number of times."
    def __init__(self, n_name, start, pattern):
        spot.python_rewriting_output.__init__(self)

        self.start   = spot.parse_rewriting_output(start)
        self.pattern = spot.parse_rewriting_output(pattern)
        self.n       = n_name

    def create(self, match):
        formulas = list(self.start.create(match))
        minimums = match.values(self.n)

        assert len(formulas) == len(minimums)

        for i, f in enumerate(formulas):
            n = minimums[i]

            for _ in range(n):
                inner_match = match.with_formula('frec', f)
                inner_formulas = self.pattern.create(inner_match)

                assert len(inner_formulas) == 1

                f = inner_formulas[0]

            formulas[i] = f
        
        return formulas

    def to_string(self, _):
        return 'repeat {} {} times'.format(self.pattern, self.n)

# Note for all the following tests:
# Sometimes, some very simple simplifications are performed by the constructors
# defined on the formula class (ie: formula.binop, formula.multop), therefore
# making some checks invalid because a formula changes.
# Additionally, sometimes these checks will result in the same simplification
# as the one we define, which means that the test succeeds even though our
# recipe wasn't defined. This can be checked when tracing is enabled, in which
# case a rewriting_match will be logged if we succeed.
# In any case, many such cases were modified to actually trigger a check

lt_tests = [
    # Formulas in lt.pdf:

    # 5.4.1. Basic simplifications

    # Basic Simplifications for Temporal Operators
    ('XFGf = FGf',              'XFGa',        'FGa'),
    ('XGFf = GFf',              'XGFa',        'GFa'),

    ('FXf = XFf',               'FXa',         'XFa'),
    ('F(f U g) = Fg',           'F(a U b)',    'Fb'),
    ('F(f M g) = F(f & g)',     'F(a M b)',    'F(a & b)'),
    ('FG(f & Xg) = FG(f & g)',  'FG(a & Xb)',  'FG(a & b)'),
    ('FG(f & Gg) = FG(f & g)',  'FG(a & Gb)',  'FG(a & b)'),

    ('GXf = XGf',               'GXa',         'XGa'),
    ('G(f R g) = Gg',           'G(a R b)',    'Gb'),
    ('G(f W g) = G(f | g)',     'G(a W b)',    'G(a | b)'),
    ('GF(f | Xg) = GF(f | g)',  'GF(a | Xb)',  'GF(a | b)'),
    ('GF(f | Fg) = GF(f | g)',  'GF(a | Fb)',  'GF(a | b)'),

    ('G(( GF(gm) or fn ) | ..) = (G(fn | ..)) | GF(gm | ..)',
     'G(a | b | c | GF(d) | GF(e))',
     'G(a | b | c) | GF(d | e)'),

    ##

    ('1 U f = Ff',              '1 U a',        'Fa'),
    ('f M 1 = Ff',              'a M 1',        'Fa'),
    ('(Xf) U (Xg) = X(f U g)',  '(Xa) U (Xb)',  'X(a U b)'),
    ('(Xf) M (Xg) = X(f M g)',  '(Xa) M (Xb)',  'X(a M b)'),
    ('(Xf) U b = b | X(b M f)', '(Xa) U b',     'b | X(b M a)'),
    ('(Xf) M b = b & X(b U f)', '(Xa) M b',     'b & X(b U a)'),
    ('f U (Gf) = Gf',           'a U Ga',       'Ga'),
    ('f M (Ff) = Ff',           'a M Fa',       'Fa'),
    ('f U (g | Gf) = f W g',    'a U (b | Ga)', 'a W b'),
    ('f M (g & Ff) = f M g',    'a M (b & Fa)', 'a M b'),
    ('f U (g & f) = g M f',     'b U (a & b)',  'a M b'),
    ('f M (g | f) = g U f',     'b M (a | b)',  'a U b'),

    ('f W 0 = Gf',              'a W 0',        'Ga'),
    ('0 R f = Gf',              '0 R a',        'Ga'),
    ('(Xf) W (Xg) = X(f W g)',  '(Xa) W (Xb)',  'X(a W b)'),
    ('(Xf) R (Xg) = X(f R g)',  '(Xa) R (Xb)',  'X(a R b)'),
    ('(Xf) W b = b | X(b W f)', '(Xa) W b',     'b | X(b W a)'),
    ('(Xf) R b = b & X(b R f)', '(Xa) R b',     'b & X(b R a)'),
    ('f W (Gf) = Gf',           'a W Ga',       'Ga'),
    ('f R (Ff) = Ff',           'a R Fa',       'Fa'),
    ('f W (g | Gf) = f W g',    'a W (b | Ga)', 'a W b'),
    ('f R (g & Ff) = f M g',    'a R (b & Fa)', 'a M b'),
    ('f W (g & f) = g R f',     'b W (a & b)',  'a R b'),
    ('f R (g | f) = g W f',     'b R (a | b)',  'a W b'),

    ##

    ('(FGf & ..) = FG(f & ..)', '(FGa) & (FGb) & (FGc)', 'FG(a & b & c)'),
    ('(Xf & ..) = X(f & ..)', '(Xa & Xb & Xc)', 'X(a & b & c)'),
    ('(Xf) & (FGg) = X(f & FGg)', 'Xa & FGb', 'X(a & FGb)'),
    ('Gf & .. = G(f & ..)', 'Ga & Gb & Gc', 'G(a & b & c)'),
    ('(f1 U f2) & (f3 U f2) = (f1 & f3) U f2',
     '(a U b) & (c U b)',
     '(a & c) U b'),
    ('(f1 U f2) & (f3 W f2) = (f1 & f3) U f2',
     '(a U b) & (c W b)',
     '(a & c) U b'),
    ('(f1 W f2) & (f3 W f2) = (f1 & f3) W f2',
     '(a W b) & (c W b)',
     '(a & c) W b'),
    ('(f1 R f2) & (f1 R f3) = f1 R (f2 & f3)',
     '(a R b) & (a R c)',
     'a R (b & c)'),
    ('(f1 R f2) & (f1 M f3) = f1 M (f2 & f3)',
     '(a R b) & (a M c)',
     'a M (b & c)'),
    ('(f1 M f2) & (f1 M f3) = f1 M (f2 & f3)',
     '(a M b) & (a M c)',
     'a M (b & c)'),
    ('Fg & (f U g) = f U g', 'Fb & (a U b)', 'a U b'),
    ('Fg & (f W g) = f U g', 'Fb & (a W b)', 'a U b'),
    ('Fg & (f R g) = f M g', 'Fb & (a R b)', 'a M b'),
    ('Fg & (f M g) = f M g', 'Fb & (a M b)', 'a M b'),
    ('f & (Xf W g) = g R f', 'a & (Xa W b)', 'b R a'),
    ('f & (Xf U g) = g M f', 'a & (Xa U b)', 'b M a'),
    ('f & (f | X(g R f)) = g R f', 'a & (a | X(b R a))', 'b R a'),
    ('f & (f | X(g M f)) = g M f', 'a & (a | X(b M a))', 'b M a'),

    ('(GFf | ..) = GF(f | ..)', '(GFa) | (GFb) | (GFc)', 'GF(a | b | c)'),
    ('(Xf | ..) = X(f | ..)', '(Xa | Xb | Xc)', 'X(a | b | c)'),
    ('(Xf) | (GFg) = X(f | GFg)', 'Xa | GFb', 'X(a | GFb)'),
    ('Ff | .. = F(f | ..)', 'Fa | Fb | Fc', 'F(a | b | c)'),
    ('(f1 U f2) | (f1 U f3) = f1 U (f2 | f3)',
     '(a U b) | (a U c)',
     'a U (b | c)'),
    ('(f1 U f2) | (f1 W f3) = f1 W (f2 | f3)',
     '(a U b) | (a W c)',
     'a W (b | c)'),
    ('(f1 W f2) | (f1 W f3) = f1 W (f2 | f3)',
     '(a W b) | (a W c)',
     'a W (b | c)'),
    ('(f1 R f2) | (f3 R f2) = (f1 | f3) R f2',
     '(a R b) | (c R b)',
     '(a | c) R b'),
    ('(f1 R f2) | (f3 M f2) = (f1 | f3) R f2',
     '(a R b) | (c M b)',
     '(a | c) R b'),
    ('(f1 M f2) | (f3 M f2) = (f1 | f3) M f2',
     '(a M b) | (c M b)',
     '(a | c) M b'),
    ('Gg | (f U g) = f W g', 'Gb | (a U b)', 'a W b'),
    ('Gg | (f W g) = f W g', 'Gb | (a W b)', 'a W b'),
    ('Gg | (f R g) = f R g', 'Gb | (a R b)', 'a R b'),
    ('Gg | (f M g) = f R g', 'Gb | (a M b)', 'a R b'),
    ('f | (Xf R g) = g W f', 'a | (Xa R b)', 'b W a'),
    ('f | (Xf M g) = g U f', 'a | (Xa M b)', 'b U a'),
    ('f | (g & X(g W f)) = g W f', 'a | (b & X(b W a))', 'b W a'),
    ('f | (g & X(g U f)) = g U f', 'a | (b & X(b U a))', 'b U a'),

    ##

    ('f & XGf = Gf', 'a & XGa', 'Ga'),
    ('f | XFf = Ff', 'a | XFa', 'Fa'),

    ('(Ff or GFg) | .. = F(f | .. | GFg | ..)',
     'Fa | Fb | Fc | GFd',
     'F(a | b | c | GFd)'),


    # Basic Simplifications for SERE Operators

    ('(f1 ; g1) && (f2 ; g2) = (f1 & f2) ; (g1 & g2)',
     '{ {a ; b} && {c ; d} }',
     '{ {a && c} ; {b && d} }'),
    
    ('(f1 : g1) && (f2 : g2) = (f1 & f2) : (g1 & g2)',
     '{ {a : b} && {c : d} }',
     '{ {a && c} : {b && d} }'),


    # Basic Simplifications SERE-LTL Binding Operators

    ('[*] []-> f = Gf'     , '{ [*] } []-> a' , 'Ga'),
    ('b[*] []-> f = f W !b', '{ b[*] } []-> a', 'a W !b'),
    ('b[+] []-> f = f W !b', '{ b[+] } []-> a', 'a W !b'),

    ('r[*0..j] []-> f = r[*1..j] []-> f',
     '{ a[*0..23] }[]-> c',
     '{ a[*1..23] }[]-> c'),
    
    ('(r ; [*]) []-> f = r []-> Gf',
     '{ a ; [*] } []-> b',
     '{ a } []-> Gb'),

    ('(r ; b[*]) []-> f = r []-> (f & X(f W !b))',
     '{ a ; b[*] } []-> c',
     '{ a } []-> (c & X(c W !b))'),
    
    ('([*] ; r) []-> f = G(r []-> f)',
     '{ [*] ; a } []-> b',
     'G({ a } []-> b)'),
    
    ('(b[*] ; r) []-> f = (!b) R (r []-> f)',
     '{ a[*] ; b } []-> c',
     '(!a) R ({ b } []-> c)'),
    
    ('(r1 ; r2) []-> f = r1 []-> X(r2 []-> f)',
     '{ a ; b } []-> c',
     '{ a } []-> X({ b } []-> c)'),
    
    # formula.multop automatically transforms '{a:b} []-> c' into 'c | !(a & b)'
    # therefore I don't exactly know how to test this
    # ('(r1 : r2) []-> f = r1 []-> (r2 []-> f)',
    #  '{ a : b } []-> c',
    #  '{ a } []-> ({ b } []-> c)'),

    # same with '{a|b} []-> c' being transformed into 'c | !(a | b)'
    # ('(r1 | r2) []-> f = (r1 []-> f) & (r2 []-> f)',
    #  '{ a | b } []-> c',
    #  '({ a } []-> c) & ({ b } []-> c)'),

    ('[*] <>-> f = Ff', '{ [*] } <>-> a', 'Fa'),
    ('b[*] <>-> f = f M b', '{ a[*] } <>-> b', 'b M a'),
    ('b[+] <>-> f = f M b', '{ a[+] } <>-> b', 'b M a'),

    ('r[*0..j] <>-> f = r[*1..j] <>-> f',
     '{ a[*0..11] } <>-> b',
     '{ a[*1..11] } <>-> b'),
    
    ('(r ; [*]) <>-> f = r <>-> Ff', '{ a ; [*] } <>-> b', '{ a } <>-> Fb'),

    ('(r ; b[*]) <>-> f = r <>-> (f | X(f M b))',
     '{a ; b[*]} <>-> c',
     '{ a } <>-> (c | X(c M b))'),
    
    ('([*] ; r) <>-> f = F(r <>-> f)',
     '{ [*] ; a } <>-> b',
     'F({ a } <>-> b)'),
    
    ('(b[*] ; r) <>-> f = b U (r <>-> f)',
     '{a[*] ; b} <>-> c',
     'a U ({b} <>-> c)'),
    
    ('(r1 ; r2) <>-> f = r1 <>-> X(r2 <>-> f)',
     '{ a ; b } <>-> c',
     '{ a } <>-> X({ b } <>-> c)'),
    
    # again '{a : b} <>-> c' is automatically transformed to 'a & b & c'
    # ('(r1 : r2) <>-> f = r1 <>-> (r2 <>-> f)',
    #  '{ a : b } <>-> c',
    #  '{ a } <>-> ({ b } <>-> c)'),

    # again '{a | b} <>-> c' is automatically transformed to 'c & (a | b)'
    # ('(r1 | r2) <>-> f = (r1 <>-> f) | (r2 <>-> f)',
    #  '{ a | b } <>-> c',
    #  '({ a } <>-> c) | ({ b } <>-> c)')

    ('r[*] = r'         , '{ a[*] }' , '{ a }'),
    ('r ; 1 = r'        , '{ a ; 1 }', '{ a }'),
    ('r ; 1 = 1'        , '{ a ; 1 }', '1'),
    ('r1 ; r2 = r1'     , '{ a ; b }', '{ a }'),
    ('r1 ; r2 = r1 | r2', '{ a ; b }', '{ a } | { b }'),

    # FIXME: Assertion fails when creating formula
    # ('b ; r = { b } & X { r }', '{ a ; b }', 'a & X{ b }'),

    (('{ b[*i..j] } = brec', None, { 'brec': FoldingOutput('i', 'b', 'b & Xfrec') }),
      '{ b[*3..5] }',
      'b & X(b & X(b & Xb))'),

    ('r1 | r2 = { r1 } | { r2 }', '{ a | b }', '{ a } | { b }'),

    ('!{ r[*] } = !r'        , '!{ a[*] }' , '!{ a }'),
    ('!{ r ; 1 } = !r'       , '!{ a ; 1 }', '!{ a }'),
    ('!{ r ; 1 } = 0'        , '!{ a ; 1 }', '0'),
    ('!{r1 ; r2} = !r1'      , '!{ a ; b }', '!{ a }'),
    ('!{r1 ; r2} = !r1 & !r2', '!{ a ; b }', '!{ a } & !{ b }'),

    ('!{b ; r} = (!b) & X!{ r }', '!{ a ; b }', '(!a) & X!{ b }'),
]

def switch(f):
    """Transforms APs into formulas according to the specified rules:
     - If the name starts with 'e', a purely eventual formula is returned,
     - If the name starts with 'u', a purely universal formula is returned,
     - If the name starts with 's' or 'q', an eventual / universal formula
       is returned.
     - If none of the previous conditions is respected, the initial AP
       is returned.
    """
    if not f._is(spot.op_ap):
        return f.map(switch)

    suffix = f.ap_name()[1:]

    if f.ap_name()[0] == 'e': return spot.formula('Fe' + suffix)
    if f.ap_name()[0] == 'u': return spot.formula('Gu' + suffix)
    if f.ap_name()[0] in ('s', 'q'): return spot.formula('GFs' + suffix)

    return f

lt_tests.extend((r, switch(spot.formula(g)), switch(spot.formula(e)))
                for (r, g, e) in [
    # 5.4.2. Simplifications for Eventual and Universal Formulas

    # It's quite hard to test for something like 'Fe = e', since the constructor
    # of formula will automatically transform 'FFe' into 'Fe'.
    # If anyone finds a good solution, simply change what functions are returned
    # by 'switch' above when an eventual and/or universal formula is needed,
    # in a way that won't let 'formula.binop' and others perform optimizations
    # ('Fe = e', 'Fe', 'e'), ('Fe = e', 'Fa', 'Fa'),
    # ('Fu | q = F(u | q)', 'Fu | q', 'F(u | q)'),
    # ('Gu = u', 'Gu', 'u'), ('Gu = u', 'Ga', 'Ga'),
    # ('Ge & q = G(e & q)', 'Ge & q', 'G(e & q)'),
    # ...

    ('G( (Xe or f) & .. ) = G( (f & ..) & (e & ..) )',
     'G(a & b & Xe & Xq)',
     'G(a & b & e & q)'),
    ('G( (Xe or f) & .. ) = G( (f & ..) & (e & ..) )',
     'G(a & b & Xc & Xq)',
     'G(a & b & Xc & q)'),
    
    # ...

])

lcc = spot.language_containment_checker()

f_implies_g     = lambda m: lcc.contained(m.g, m.f)
f_implies_not_g = lambda m: lcc.contained(spot.formula.Not(m.g), m.f)
f_equiv_g       = lambda m: lcc.are_equivalent(m.f, m.g)
f_lt_g          = lambda m: len(m.f) < len(m.g)
g_lt_f          = lambda m: len(m.g) < len(m.f)

and_ = lambda a, b: lambda m: a(m) and b(m)

lt_tests.extend(((r, None, None, cond), g, e) for (r, cond, g, e) in [
    # 5.4.3. Simplifications based on Implications

    ('f | g = 1', f_implies_not_g, 'f | !f', '1'),
    ('f & g = 0', f_implies_not_g, 'f & !f', '0'),
    ('f | g = f', and_(f_equiv_g, f_lt_g), '(a & b) | (a & b & !(!b | !a))', '(a & b)'),
    ('f | g = g', f_implies_g, 'Gf | f', 'Gf'),
    ('f & g = g', and_(f_equiv_g, g_lt_f), '(a | Ga | !(!Ga & !a)) & (Ga | a)', 'a | Ga'),
    ('f & g = f', f_implies_g, 'a & Ga', 'a'),

    # Gotta find a good example here:
    # ('f <-> g = g -> f', f_implies_g, 'a <-> b', 'b -> a')
])

for recipe, given, expected in lt_tests:
    print('Rewriting ', CB, str(given).rjust(LPAD), CEND, ' into ', CB, expected, CEND, sep='')
    rewritten = rewrite(given, recipe)

    if rewritten != expected:
        print()
        print('Formula \'{}{}{}\' wrongly rewritten'.format(CR, given, CEND))
        print('-> Expected \'{}{}{}\' but got \'{}{}{}\''.format(CR, expected, CEND, CR, rewritten, CEND))
        print()
        print('Recipe:\n')
        print(CR, parse_recipe(recipe), CEND, sep='')

        exit(1)
    
print()
print(CG, 'All tests successfully passed.', CEND, sep='')

import {test} from 'node:test';
import assert from 'node:assert/strict';
import {passwordAllowed,passwordScore} from '../src/lib/account-policy.ts';

test('password feedback rejects repeated and obvious values despite their length',()=>{
  for(const value of ['aaaaaaaaaaaaaaaaaaaaaaaa','password password password','Password123456789!','qwerty123456789!']){
    assert.ok(passwordScore(value)<3,value);
  }
});
test('password feedback accepts a long passphrase or a sufficiently long mixed password',()=>{
  assert.equal(passwordScore('correct horse battery staple'),4);
  assert.ok(passwordScore('Violet!River42')>=3);
  assert.ok(passwordScore('abcdefghijklmnopqrst')<3);
  assert.ok(passwordScore('Ab3!xY7?')<3);
  assert.equal(passwordAllowed('a-long-quiet-river'),true);
  assert.equal(passwordAllowed('abcdefghijklmnop'),true);
  assert.equal(passwordAllowed('Violet!River42'),false);
  assert.equal(passwordAllowed('aaaaaaaaaaaaaaaaaaaa'),false);
});

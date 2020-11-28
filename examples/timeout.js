import * as uv from '../tuv.so'

print('uv.cwd', uv.cwd());

const sec = 2;
print('wait for ' + sec + ' seconds');
uv.setTimeout(() => {
  print('timeout reached');
}, sec * 1000);
uv.setTimeout(() => {
  print('after 1 sec');
}, 1000);

uv.run();
print('end run loop');

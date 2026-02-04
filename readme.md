`WS_EX_LAYERED` 위에 D2D와 Sol 올려서 랜더링하는 게임 엔진입니다.

love2d를 바라보고 개발하고 있습니다.

## Requirement
- lua 5.4
- sol
- visual studio

## How to run
생성된 exe 파일은 main.lua를 읽습니다.  
실행 직후 글로벌 `Init()`을 실행합니다.  
매 프레임 글로벌 `Update(dtms)`와 `Draw()`를 실행합니다.  
사용자 입력에 의해
`OnKeyDown(keycode)`, `OnKeyUp(keyCode)`,
`OnMouseDown(x, y)`, `OnMouseUp(x, y)`,
`OnRightMouseDown(x, y)`, `OnRightMouseUp(x, y)`
를 실행합니다.

g, input, res, sys 테이블이 그리기용으로 바인드되었습니다.

[개발중인 게임 소스](https://github.com/hyuckkim/Carriage)
라도 참고하실래요...? 보기 좀 많이 더럽습니다

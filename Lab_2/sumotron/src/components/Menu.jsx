import { useState, useEffect, use } from 'react'

export function Menu() {
  const [firstNumber, setFirstNumber] = useState('');
  const [secondNumber, setSecondNumber] = useState('');
  const [sum, setSum] = useState(0);

  const handleKeyDown = (event) => {
    switch (event.key) {
      case 'ArrowUp':
        handleForward(event);
        break;
      case 'ArrowDown':
        handleBackward(event);
        break;
      case 'ArrowLeft':
        handleLeft(event);
        break;
      case 'ArrowRight':
        handleRight(event);
        break;
    }
  };


  const handleForward = (event) => {
    console.log('Forward button clicked');
  }

  const handleBackward = (event) => {
    console.log('Backward button clicked');
  }

  const handleLeft = (event) => {
    console.log('Left button clicked');
  }

  const handleRight = (event) => {
    console.log('Right button clicked');
  }
    return (
        <div className="menu">
            <h1>Sumotron</h1>
            <div>
                <input 
                type='text'
                onKeyDown={handleKeyDown}
                />
            </div>
        </div>
    );
}
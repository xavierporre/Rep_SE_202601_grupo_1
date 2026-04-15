import { useState, useEffect, use } from 'react'

export function Menu() {

    useEffect(() => {
      const handleKeyDown = (event) => {
        switch (event.key) {
          case 'w':
            handleForward(event);
            break;
          case 's':
            handleBackward(event);
            break;
          case 'a':
            handleLeft(event);
            break;
          case 'd':
            handleRight(event);
            break;
        }
      };
      window.addEventListener('keydown', handleKeyDown);
      return () => {
        window.removeEventListener('keydown', handleKeyDown);
      };
    }, []);

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
            </div>
        </div>
  );
}
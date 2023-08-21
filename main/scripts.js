



hide_div("test_div");

//ADJUST COLOR
const colorInput = document.getElementById('color');
const lights = document.querySelectorAll('.running_lights ul li');
const lights_circle = document.querySelectorAll('.circle');
const circleContainers = document.querySelectorAll('.circle-container');

      circleContainers.forEach(container => {
      changeBoxShadowColorInAnimation(container, '#ffff00');
  });
  

  colorInput.addEventListener('input', () => {
    const newColor = colorInput.value;
    for (let i = 0; i < lights.length; i++) {
      lights_circle[i].style.backgroundColor = `${newColor}`;
    }
    circleContainers.forEach(container => {
      changeBoxShadowColorInAnimation(container, newColor);
    });
  });




// Repeat for other circles

// Function to change box shadow color within the animation
function changeBoxShadowColorInAnimation(element, color) {
  element.style.setProperty('--box-shadow-color', color);
}
//END OF ADJUST COLOR


//CHANGE SPEED
const speedSlider = document.getElementById('speed');
console.log("speed id = ",speed);

const circles = document.querySelectorAll('.circle');

function updateAnimationSpeed() {
  console.log("adjusting speed ");
  const animationSpeed = 6 - speedSlider.value; // Inverse mapping from slider value to animation speed
  circles.forEach(circle => {
    circle.style.animation = `grow ${animationSpeed}s ease-in-out infinite`;
  });
}

speedSlider.addEventListener('input', updateAnimationSpeed);
//END OF CHANGE SPEED





function user_mode_popup_configure(sel) {
  hide_div("test_div");

}

function user_mode_popup_cancel(sel) {
  hide_div("test_div");
}


function hide_div(div) {
  console.log("hiding div");
  var document_div = document.getElementById(div);
  document_div.setAttribute("hidden", "hidden");
}

function unhide_div(div) {
  console.log("unhiding div");
  var document_div = document.getElementById(div);
  document_div.removeAttribute("hidden");
}
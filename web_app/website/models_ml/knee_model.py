import torch.nn as nn

class KneeFlexionModel(nn.Module):
  def __init__(self):
    super().__init__()

    self.linear =nn.Sequential(
      nn.ReLU(), 
      nn.Linear(4, 32),
      nn.ReLU(),
      nn.Linear(32, 64),
      nn.ReLU(),
      nn.Linear(64, 100)
      )
              
  def forward(self, x):
    
    x = self.linear(x)
    
    return x
  